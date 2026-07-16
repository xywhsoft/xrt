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

---

## 6. Stable Core Contract

### TCP

- Hostname connects resolve all IPv4/IPv6 candidates and fall back within one total `iConnectTimeoutMs` deadline.
- `iFallbackDelayMs` defaults to 250 ms. Each attempt has a unique generation, so a late completion from an abandoned socket cannot affect the active attempt.
- The connect deadline covers DNS, TCP, proxy negotiation, and TLS handshake.
- `iHighWater` and `iLowWater` are notification thresholds; `iMaxQueuedBytes` is a hard limit. A send that would exceed it returns `XRT_NET_AGAIN` without accepting a partial input.
- `xrtNetStreamShutdownWrite()` drains accepted output, closes the write direction, and keeps reading until the peer closes.
- `xrtNetStreamAbort()` immediately discards queues and aborts the socket.

### UDP

- Both unconnected `SendTo/RecvFrom` and connected `Send/Recv` are supported.
- `iSendQueueLimit` is a hard pending-byte budget. `SendBatch` stops at the first item that cannot be accepted and reports the accepted count.
- `iRecvBatch` controls native receive preposts. `iRecvQueueLimit` and the overflow policy bound the user-space receive queue.
- A receive batch owns packets that have not been taken. A taken packet is owned by the caller.
- Broadcast, IPv6-only/dual-stack, socket buffers, hop limit, traffic class, and multicast controls are explicit options.

## 7. Ownership and Buffers

- Legacy `xfuture` compatibility APIs retain their boxed result ABI: `TcpConnectAsync`, `TcpAcceptAsync`, and `UdpRecvAsync` expose a pointer-to-handle through `xFutureValue`; `RecvTextAsync` exposes `char**`; send and drain expose `int*`. The Future owns only the box, not the returned network object.
- The new bytes API returns a Future-owned `xnetbytes*`. Native `xnetfuture` network APIs return their documented object pointers directly and do not use compatibility boxing. These result shapes must not be mixed.
- `Send` and `SendVec` copy their inputs. `SendRef` does not copy; its memory must remain valid until the release callback, which is invoked exactly once after a successful submission.
- A synchronous `xrtNetResolveAll` result is caller-owned. A `xrtNetResolveAllFuture` result is Future-owned.
- Datagram packets and packets taken from a batch are caller-owned. Untaken packets are destroyed with the batch.
- Destroying a stream, listener, or datagram object with outstanding internal operations defers reclamation. The caller must not access the object after `Destroy`.
- Network objects no longer carry a fixed 8 KiB receive buffer. Received bytes use per-worker size-classed blocks/chains, with dynamic allocation only when required.
- `xnetmemctx` and directly manipulated chains are thread-affine while active.

## 8. Threading, Waiting, and Cancellation

- Every network object belongs to one engine worker. Completions and user callbacks are serialized on that worker.
- Cross-thread operations are posted to the owner worker. `XRT_NET_OK` means accepted, not completed.
- Callbacks must not block a worker. CPU-bound or blocking work belongs on the bounded `xtaskexecutor`.
- Timeout is relative; deadline uses a monotonic absolute clock.
- A wait timeout or cancellation ends that waiter unless an API explicitly states that it aborts the underlying object. Cancelling a TCP connect Future aborts its unopened stream.
- Completion, timeout, and cancellation race to one terminal Future result; losing completion paths release transferred values.
- Resource pressure is `XRT_NET_AGAIN`; peer close is `XRT_NET_CLOSED`; timeout and cancellation are `XRT_NET_TIMEOUT` and `XRT_NET_CANCELLED`.

## 9. Configuration ABI

- Public configuration structures begin with `iSize` and `iVersion` and must be initialized with their matching `ConfigInit` function.
- Version 1 accepts `iSize >= *_CONFIG_V1_SIZE` and reads only the known prefix present in the caller's size.
- New fields may only be appended. Existing fields cannot be reordered, removed, or changed in type.
- A V1 caller receives defaults for appended fields. A current library ignores an unknown tail from a future larger structure.
- Unknown versions and structures smaller than V1 are rejected.

## 10. Backend Levels

| Backend | Level | Contract |
|---|---:|---|
| Windows IOCP | Tier A | Native connect/accept/recv/send and batched completion; current primary release validation platform |
| Linux io_uring | Tier A target | Native async path; must pass the same Linux regression, stress, and leak matrix before a Linux release claims Tier A |
| Linux epoll / BSD kqueue | Tier B | Readiness fallback with the same API semantics but no Tier A peak-throughput guarantee |
| select | Tier C | Compatibility fallback for lower connection counts |

Backends may differ in batching and throughput, but not in ownership, errors, cancellation, backpressure, or close semantics.

## 11. Release Gate

- TCP: echo, slow reader/writer, hard backpressure, half-close, RST, multi-address fallback, timeout, and cancellation.
- Listener: concurrent accepts, bounded accept queue, timeout boundaries, and stop/destroy races.
- UDP: connected/unconnected operation, send/receive batches, bursts, overflow policies, and multicast options.
- Runtime: command fairness, timer catch-up, bounded task execution, cancellation, destroy, and engine live-object guards.
- Memory: pending bytes, async holds, packet/batch ownership, ref releases, and backend active IO all return to zero after shutdown.
- A platform tier is claimed only after this matrix passes on that platform.
