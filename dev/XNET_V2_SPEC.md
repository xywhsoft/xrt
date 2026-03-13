# XNET V2 Rewrite Specification

## 1. Document Control

- Document: `dev/XNET_V2_SPEC.md`
- Status: Active draft
- Version: `0.1.28`
- Last updated: `2026-03-13`
- Scope: Full rewrite of the xrt network infrastructure
- Source of truth: This file is the primary development spec for `xnet-v2`

### 1.1 Purpose

This document exists to prevent design drift during a long-running rewrite.
It records the target architecture, module boundaries, invariants, API shape,
milestones, acceptance gates, risks, and the work tracking model.

If future work happens in a new chat or after long pauses, development must
resume from this file first.

### 1.2 Update Rules

The following rules are mandatory:

1. Every material design change must update this file before or with code.
2. Every completed task must update the progress checklist in Section 15.
3. Every architecture decision that changes behavior, scope, or tradeoffs must
   add an item to Section 19.
4. Every work session should append a short entry to Section 20.
5. New code is not considered stable until its milestone exit criteria are met.

### 1.3 Status Legend

- `todo`: Not started
- `in_progress`: Active work
- `blocked`: Waiting on a decision, dependency, or bug fix
- `done`: Implemented and accepted against milestone criteria
- `deferred`: Explicitly pushed out of the current rewrite scope


## 2. Rewrite Summary

`xnet-v1` is not being incrementally improved. It is being frozen and replaced.

The current stack has structural limits that prevent it from becoming a
top-tier high-concurrency core:

- transport hot paths still depend on per-op fixed buffers
- send paths still copy too often
- receive paths still depend on linear consume patterns
- protocol layers maintain their own transport logic
- sync and async paths are duplicated instead of sharing one core
- TLS and HTTP use transport-adjacent buffering patterns that do not scale

The rewrite strategy is:

1. Freeze current network infrastructure into a `dev/net-v1/` backup area.
2. Build `xnet-v2` as a new async-first kernel.
3. Rebuild upper protocols on top of `xnet-v2`.
4. Provide sync client APIs as facades over the same async core.


## 3. Goals

### 3.1 Product Goals

`xnet-v2` must support both:

- high-performance server development
- convenient network client development

### 3.2 Technical Goals

- Async-first transport core for TCP and UDP
- Windows backend based on IOCP
- Linux backend based on io_uring
- No external runtime dependencies beyond libc and platform wrappers
- Builtin TLS retained and integrated as the only in-tree TLS provider
- Sync protocol APIs implemented on top of the async core
- Low idle memory per connection
- No large-scale hot-path memmove
- No per-send heap allocation as the default transport behavior
- Strong backpressure and slow-peer handling
- Clear ownership and threading guarantees
- Support for IPv4 and IPv6 in the new base model
- Compatibility with future Linux, board, and RTOS platform adapters

### 3.3 Rewrite Success Criteria

The rewrite is successful only if all of the following are true:

- one transport core serves both async and sync protocol APIs
- HTTP client, HTTP server, and WebSocket no longer own transport logic
- no protocol layer calls raw socket APIs directly
- per-connection memory is demand-driven instead of fixed-buffer-driven
- worker ownership is deterministic and stable
- benchmarks prove a clear improvement over `xnet-v1`


## 4. Non-Goals

The following are not required for the first rewrite wave:

- HTTP/2
- QUIC
- kernel bypass frameworks
- cross-process shared engines
- external TLS engines such as OpenSSL, mbedTLS, or wolfSSL
- preserving `xnet-v1` internals
- retaining the old poller abstraction shape


## 5. Constraints

- The codebase must remain dependency-light and portable.
- The final public API must still fit xrt's single-header distribution model.
- The transport core must not assume a desktop-only environment.
- Linux and Windows are primary implementation targets for the rewrite.
- RTOS and board support are architectural targets, even if not fully delivered
  in phase 1.
- Builtin TLS remains the only supported TLS implementation inside the tree.


## 6. Design Principles

1. One async core, many facades.
2. One connection belongs to exactly one worker at a time.
3. No global lock on the hot path.
4. No transport-level memmove of large live receive buffers.
5. No hidden per-op 8 KB or similar fixed payload buffers.
6. No protocol-owned transport threads.
7. Backpressure is mandatory, not optional.
8. TLS is a stream filter, not a parallel transport stack.
9. Sync APIs wait on async completion objects.
10. Protocols consume chains, not raw socket reads.


## 7. Repository Layout Plan

### 7.1 Legacy Freeze

The existing network stack will be moved or copied into a frozen backup area:

- target path: `dev/net-v1/`
- keep tests and examples that document current behavior
- add a freeze note and date
- keep old code available for diff and behavior reference only

### 7.2 New Code Layout

New code should be introduced under `lib/` and `test/` with a distinct naming
scheme:

- `lib/xnet_base.h`
- `lib/xnet_mem.h`
- `lib/xnet_port.h`
- `lib/xnet_port_iocp.h`
- `lib/xnet_port_uring.h`
- `lib/xnet_engine.h`
- `lib/xnet_stream.h`
- `lib/xnet_dgram.h`
- `lib/xnet_sync.h`
- `lib/xnet_tls.h`
- `lib/xcodec.h`
- `lib/xcodec_http1.h`
- `lib/xcodec_ws.h`
- `lib/xws2.h`
- `test/test_xnet2_base.h`
- `test/test_xnet2_engine.h`
- `test/test_xnet2_mem.h`
- `test/test_xnet2_port.h`
- `test/test_xnet2_stream.h`
- `test/test_xnet2_dgram.h`
- `test/test_xnet2_tls.h`
- `test/test_xnet2_sync.h`
- `test/test_xnet2_codec.h`
- `test/test_xnet2_ws2.h`
- `dev/test_xnet2_stage.c`
- `dev/bench/xnet2/bench_common.h`
- `dev/bench/xnet2/bench_stream_server.h`
- `dev/bench/xnet2/bench_idle_conn.c`
- `dev/bench/xnet2/bench_echo_tcp.c`
- `dev/bench/xnet2/bench_echo_tls.c`
- `dev/bench/xnet2/bench_udp_burst.c`
- `dev/bench/xnet2/bench_send_queue_pressure.c`
- `dev/bench/xnet2/*`


## 8. Module Inventory

| Module | Phase | Status | Responsibility | Hard Dependencies |
| --- | --- | --- | --- | --- |
| `xnet_base` | 1 | done | Common enums, flags, address types, helper structs | none |
| `xnet_mem` | 1 | done | Block pools, chains, ref-send buffers | `xnet_base` |
| `xnet_port` | 1 | done | Backend-neutral I/O interface | `xnet_base` |
| `xnet_port_iocp` | 1 | in_progress | Windows IOCP backend | `xnet_port` |
| `xnet_port_uring` | 1 | in_progress | Linux io_uring backend | `xnet_port` |
| `xnet_engine` | 1 | done | Engine, workers, command queues, timers | `xnet_base`, `xnet_mem`, `xnet_port` |
| `xnet_stream` | 1 | done | TCP listener and stream transport | `xnet_engine` |
| `xnet_dgram` | 1 | done | UDP and packet transport | `xnet_engine` |
| `xnet_sync` | 1 | done | Future and waiter model for sync facades | `xnet_engine` |
| `xnet_tls` | 2 | done | Builtin TLS integration on stream transport | `xnet_stream` |
| `xcodec_*` | 2 | done | Framing and parser helpers | `xnet_stream` |
| `xhttp2` | 3 | done | New HTTP client | `xnet_stream`, `xnet_tls`, `xnet_sync` |
| `xhttpd2` | 3 | done | New HTTP server | `xnet_stream`, `xnet_tls`, `xcodec_http1` |
| `xws2` | 3 | done | New WebSocket layer | `xnet_stream`, `xcodec_http1`, `xcodec_ws`, `xnet_tls` |


## 9. Core Data Model

This section defines the primary objects. Exact field sets may evolve, but the
roles and invariants are part of the spec.

### 9.1 Base Public Types

```c
typedef struct xrt_net_engine   xnetengine;
typedef struct xrt_net_worker   xnetworker;
typedef struct xrt_net_listener xnetlistener;
typedef struct xrt_net_stream   xnetstream;
typedef struct xrt_net_dgram    xdgramsock;
typedef struct xrt_net_chain    xnetchain;
typedef struct xrt_net_future   xnetfuture;
typedef struct xrt_tls_session  xtlssession;

typedef struct {
    uint16 iFamily;
    uint16 iPort;
    uint32 iScopeId;
    uint8  aAddr[16];
} xnetaddr;

typedef struct {
    const void* pData;
    uint32 iLen;
} xnetspan;

typedef struct {
    const void* pData;
    uint32 iLen;
    void (*pfnRelease)(ptr pCtx, const void* pData, size_t iLen);
    ptr pReleaseCtx;
} xnetbufref;
```

### 9.2 Engine Config

```c
typedef struct {
    uint32 iWorkerCount;
    uint32 iFlags;
    uint32 iSqEntries;
    uint32 iCqEntries;
    uint32 iAcceptBatch;
    uint32 iCmdQueueSize;
    uint32 iTimerTickMs;
    uint32 iTimerWheelSlots;
    uint32 iDefaultHighWater;
    uint32 iDefaultLowWater;
    uint32 iSmallBlockSize;
    uint32 iMediumBlockSize;
    uint32 iLargeBlockSize;
    uint32 iBlockCachePerWorker;
    uint32 iMaxConnsPerWorker;
} xnetengineconfig;
```

Default targets for the first implementation:

- `iWorkerCount = 0` means auto
- `iSqEntries = 4096`
- `iCqEntries = 8192`
- `iAcceptBatch = 64`
- `iCmdQueueSize = 65536`
- `iTimerTickMs = 10`
- `iTimerWheelSlots = 4096`
- `iDefaultHighWater = 262144`
- `iDefaultLowWater = 65536`
- `iSmallBlockSize = 256`
- `iMediumBlockSize = 2048`
- `iLargeBlockSize = 16384`

### 9.3 Internal Block Model

```c
typedef struct __xnet_blk {
    struct __xnet_blk* pNext;
    uint16 iClassId;
    uint16 iRefCount;
    uint32 iBegin;
    uint32 iEnd;
    uint32 iCapacity;
    uint8  aData[1];
} __xnet_blk;

struct xrt_net_chain {
    __xnet_blk* pHead;
    __xnet_blk* pTail;
    uint32 iBytes;
    uint32 iBlockCount;
};
```

Block and chain invariants:

- chains are append-only until consume
- consume advances offsets and frees whole blocks when exhausted
- consume never memmoves user payload between live blocks
- a block may be ref-counted for send-ref paths
- a block may either own inline payload or hold an external ref-send payload
- ref-send blocks are non-cacheable and release caller-owned payloads via callback
- a chain is not thread-safe outside its owner worker

### 9.4 Stream Send Queue

```c
typedef struct {
    xnetchain tQueue;
    uint32 iQueuedBytes;
    uint32 iHighWater;
    uint32 iLowWater;
    bool bHighWaterHit;
    bool bWritePosted;
} __xnet_sendq;
```

Send queue invariants:

- queue order is FIFO
- only the owner worker mutates the queue
- `bWritePosted` prevents duplicate backend write submission storms
- high water and low water callbacks fire on transitions, not on every send

### 9.5 Stream Object

```c
struct xrt_net_stream {
    uint64 iId;
    xsocket hSocket;
    xnetworker* pWorker;
    xnetlistener* pListener;
    xtlssession* pTls;
    xnetaddr tLocalAddr;
    xnetaddr tRemoteAddr;
    xnetchain tRxChain;
    __xnet_sendq tSendQ;
    ptr pUserData;
    uint32 iState;
    uint32 iFlags;
    uint32 iRecvLimit;
    bool bReadPaused;
    bool bClosing;
};
```

Stream invariants:

- one stream belongs to exactly one worker after creation
- all stream callbacks execute on the owner worker thread
- `tRxChain` holds bytes not yet consumed by the upper layer
- closing a stream disallows new outbound data
- a stream emits exactly one terminal close callback

### 9.6 Listener Object

```c
struct xrt_net_listener {
    xnetengine* pEngine;
    ptr pUserData;
    xnetlistenconfig tConfig;
    uint32 iPortCount;
    uint32 iNextWorker;
    bool bRunning;
};
```

Listener invariants:

- listener owns only accept state, not client transport state
- accepted streams are assigned to workers deterministically
- Linux may use per-worker sockets with `SO_REUSEPORT`
- Windows may accept on one socket and dispatch accepted sockets by worker

### 9.7 Worker Object

```c
struct xrt_net_worker {
    uint32 iId;
    xthread hThread;
    volatile bool bRunning;
    ptr pPortCtx;
    ptr pCmdQ;
    ptr pTimerWheel;
    ptr pSlabCache;
    ptr pStreamTable;
    ptr pDgramTable;
};
```

Worker invariants:

- a worker is the exclusive owner of all transport state assigned to it
- cross-thread actions become commands queued to the owner worker
- worker-local memory caches are not shared directly across workers
- worker threads are created through `xrt` runtime-managed thread APIs so they inherit attach/detach lifecycle and thread-local runtime state

### 9.8 Future Object

```c
struct xrt_net_future {
    xmutex hLock;
    xcond hCond;
    volatile bool bDone;
    xnet_result iStatus;
    ptr pValue;
};
```

Future invariants:

- a future transitions from pending to done once
- pending status is reported as `XRT_NET_AGAIN`
- waiters may block on a cond or custom OS primitive
- a timed-out wait does not consume the future result; later waits may still observe completion
- sync facades expose only the future result, not engine internals


## 10. Threading and Ownership Model

- Engine owns workers.
- Worker owns streams, UDP sockets, timers, and backend submission state.
- User callbacks run only on worker threads, except sync wait completion paths.
- Cross-thread APIs must enqueue commands instead of touching transport state.
- Public getters that expose mutable objects must either be owner-thread-only or
  return immutable snapshots.

Required rule:

- No transport object may be mutated from a thread that does not own it.
- Any thread that executes worker callbacks or touches worker-owned transport state must already be attached to the XRT runtime.


## 11. Memory Model

### 11.1 Allocation Strategy

- Use worker-local slab caches for common block sizes.
- Use large dynamic blocks only when payload size exceeds slab classes.
- Use ref-send buffers for zero-copy or caller-owned payload lifetimes.
- Represent ref-send payloads as explicit reference blocks with release callbacks.
- Recycle blocks aggressively after chain consume and completed sends.

### 11.2 Hot Path Rules

- No per-send heap allocation for normal sends under common payload sizes.
- No memmove of in-flight transport payload on receive.
- No per-recv fixed buffer embedded in operation structs.
- No protocol-specific raw buffer duplication when transport already owns data.

### 11.3 Backpressure Rules

- Every stream has high water and low water thresholds.
- Crossing high water may pause read or trigger upper-layer throttling.
- Crossing low water may resume read and trigger drain callbacks.
- Slow peers must be observable and evictable.


## 12. I/O Backend Model

### 12.1 Backend Interface

`xnet_port` is the internal interface between `xnet_engine` and platform
backends. It hides submission, completion harvest, accept posting, recv posting,
send posting, wakeup, and timer wake integration.

During staging, backend skeletons may synthesize completion events for submitted
ops so queueing, worker ownership, and protocol-facing transport semantics can
be validated before raw socket wiring is attached.

### 12.2 Windows Backend

- Use IOCP
- Use `AcceptEx`
- Batch completion harvest
- Avoid per-accept socket churn where possible
- Use gather send when queue has multiple spans

### 12.3 Linux Backend

- Use io_uring
- Prefer multishot accept and receive when available
- Prefer provided buffer groups when kernel support is sufficient
- Fall back to single-shot io_uring behavior without changing upper layers

### 12.4 RTOS / Board Portability

The architecture must not hardcode IOCP or io_uring assumptions into stream,
UDP, TLS, or protocol code. Platform-specific code must remain behind
`xnet_port`.


## 13. Transport Semantics

### 13.1 Stream Semantics

- `OnRecv` receives a mutable chain view
- upper layers parse what they can and consume what they used
- unread bytes remain in the chain for the next callback
- paused reads may accumulate bytes in the receive chain until resume dispatch
  runs on the owner worker
- sends are queued, not assumed complete at call time
- graceful close may wait for queued outbound bytes to drain before emitting the
  terminal close callback
- close supports graceful and abortive modes
- half-close support is optional in phase 1 and explicit if implemented

### 13.2 UDP Semantics

- UDP remains packet-oriented
- no fake connection model at the transport layer
- receive callback includes source address plus packet chain
- batching is supported internally even if the public API starts simple

### 13.3 TLS Semantics

- TLS is attached to a stream, not to raw sockets from protocol code
- TLS handshake is async and worker-owned
- plaintext enters send queue above TLS
- ciphertext leaves through backend send submission below TLS
- protocol code never interacts with TLS record buffers directly

### 13.4 Sync Facade Semantics

- sync APIs are wrappers over async operations plus futures
- sync HTTP must not have its own socket, TLS, or parser stack
- a hidden engine may be used for convenience APIs
- the hidden convenience engine is process-global and defaults to one worker
- advanced users may provide their own engine

### 13.5 Codec and Parser Semantics

- codec parsers consume `xnetchain` inputs and do not touch raw socket APIs
- parser results are expressed as `ERROR`, `NEED_MORE`, or `FRAME`
- `xcodecframe` describes header size, payload offset, payload bytes, and total frame bytes
- frame consumers may peek payload bytes and consume whole frames without copying transport-owned buffers
- the HTTP/1 skeleton parses start-line and headers, resolves fixed-length bodies, and intentionally leaves chunk-body decoding to later protocol work
- the WebSocket skeleton parses one frame at a time and intentionally leaves message reassembly to the upper WebSocket layer


## 14. Phase 1 Public API Shape

The following API set is the phase 1 target. Names may be refined, but the
capabilities are in scope.

```c
XXAPI void xrtNetEngineConfigInit(xnetengineconfig* pCfg);
XXAPI void xrtNetListenConfigInit(xnetlistenconfig* pCfg);
XXAPI void xrtNetConnectConfigInit(xnetconnectconfig* pCfg);
XXAPI void xrtNetDgramConfigInit(xnetdgramconfig* pCfg);

XXAPI void xrtNetAddrInitAny(xnetaddr* pAddr, int iFamily, uint16 iPort);
XXAPI xnet_result xrtNetAddrParse(xnetaddr* pAddr, const char* sIP, uint16 iPort);
XXAPI xnet_result xrtNetResolve(const char* sHost, xnetaddr* pAddr);
XXAPI const char* xrtNetAddrToStr(const xnetaddr* pAddr);

typedef void (*xnet_task_fn)(xnetworker* pWorker, ptr pArg);

XXAPI xnetengine* xrtNetEngineCreate(const xnetengineconfig* pCfg);
XXAPI void xrtNetEngineDestroy(xnetengine* pEngine);
XXAPI xnet_result xrtNetEngineStart(xnetengine* pEngine);
XXAPI void xrtNetEngineStop(xnetengine* pEngine);
XXAPI uint32 xrtNetEngineGetWorkerCount(xnetengine* pEngine);
XXAPI xnet_result xrtNetEnginePost(xnetengine* pEngine, uint32 iAffinityKey,
    xnet_task_fn pfnTask, ptr pArg);
XXAPI xnet_result xrtNetEnginePostDelayed(xnetengine* pEngine, uint32 iAffinityKey,
    uint32 iDelayMs, xnet_task_fn pfnTask, ptr pArg);

XXAPI bool xrtNetChainAppendCopy(xnetchain* pChain, const void* pData, size_t iLen);
XXAPI bool xrtNetChainAppendRef(xnetchain* pChain, const xnetbufref* pRef);
XXAPI size_t xrtNetChainBytes(const xnetchain* pChain);
XXAPI uint32 xrtNetChainSpanCount(const xnetchain* pChain);
XXAPI uint32 xrtNetChainGetSpans(const xnetchain* pChain, xnetspan* pOut, uint32 iMaxCount);
XXAPI size_t xrtNetChainPeek(const xnetchain* pChain, ptr pOut, size_t iLen);
XXAPI size_t xrtNetChainFindByte(const xnetchain* pChain, uint8 ch, size_t iStartOff);
XXAPI void xrtNetChainConsume(xnetchain* pChain, size_t iLen);

typedef struct {
    bool (*OnAccept)(ptr pOwner, xnetlistener* pListener, xnetstream* pStream);
    void (*OnError)(ptr pOwner, xnetlistener* pListener, int iSysErr);
} xnetlistenerevents;

typedef struct {
    void (*OnOpen)(ptr pOwner, xnetstream* pStream);
    void (*OnRecv)(ptr pOwner, xnetstream* pStream, xnetchain* pChain);
    void (*OnDrain)(ptr pOwner, xnetstream* pStream);
    void (*OnClose)(ptr pOwner, xnetstream* pStream, xnet_result iReason);
    void (*OnError)(ptr pOwner, xnetstream* pStream, int iSysErr);
    void (*OnHighWater)(ptr pOwner, xnetstream* pStream, uint32 iQueuedBytes);
    void (*OnLowWater)(ptr pOwner, xnetstream* pStream, uint32 iQueuedBytes);
} xnetstreamevents;

XXAPI xnetlistener* xrtNetListenerCreate(xnetengine* pEngine,
    const xnetlistenconfig* pCfg,
    const xnetlistenerevents* pEvents,
    const xnetstreamevents* pStreamEvents,
    ptr pUserData);
XXAPI void xrtNetListenerDestroy(xnetlistener* pListener);
XXAPI xnet_result xrtNetListenerStart(xnetlistener* pListener);
XXAPI void xrtNetListenerStop(xnetlistener* pListener);

XXAPI xnetstream* xrtNetStreamCreate(xnetengine* pEngine,
    const xnetstreamevents* pEvents, ptr pUserData);
XXAPI void xrtNetStreamDestroy(xnetstream* pStream);
XXAPI xnet_result xrtNetStreamConnect(xnetstream* pStream,
    const xnetconnectconfig* pCfg);
XXAPI void xrtNetStreamClose(xnetstream* pStream, uint32 iFlags);
XXAPI xnet_result xrtNetStreamSend(xnetstream* pStream, const void* pData, size_t iLen);
XXAPI xnet_result xrtNetStreamSendVec(xnetstream* pStream, const xnetspan* pVec, uint32 iCount);
XXAPI xnet_result xrtNetStreamSendRef(xnetstream* pStream, const xnetbufref* pRef);
XXAPI void xrtNetStreamPauseRead(xnetstream* pStream);
XXAPI void xrtNetStreamResumeRead(xnetstream* pStream);
XXAPI size_t xrtNetStreamPendingSend(const xnetstream* pStream);
XXAPI const xnetaddr* xrtNetStreamLocalAddr(const xnetstream* pStream);
XXAPI const xnetaddr* xrtNetStreamRemoteAddr(const xnetstream* pStream);
XXAPI void xrtNetStreamSetUserData(xnetstream* pStream, ptr pData);
XXAPI ptr xrtNetStreamGetUserData(xnetstream* pStream);

typedef struct {
    void (*OnRecv)(ptr pOwner, xdgramsock* pSock, const xnetaddr* pFrom, xnetchain* pChain);
    void (*OnError)(ptr pOwner, xdgramsock* pSock, int iSysErr);
} xnetdgramevents;

XXAPI xdgramsock* xrtNetDgramCreate(xnetengine* pEngine,
    const xnetdgramconfig* pCfg,
    const xnetdgramevents* pEvents, ptr pUserData);
XXAPI void xrtNetDgramDestroy(xdgramsock* pSock);
XXAPI xnet_result xrtNetDgramStart(xdgramsock* pSock);
XXAPI void xrtNetDgramStop(xdgramsock* pSock);
XXAPI xnet_result xrtNetDgramSendTo(xdgramsock* pSock,
    const xnetaddr* pTo, const void* pData, size_t iLen);
XXAPI xnet_result xrtNetDgramSendVecTo(xdgramsock* pSock,
    const xnetaddr* pTo, const xnetspan* pVec, uint32 iCount);

XXAPI xnetfuture* xrtNetFutureCreate(void);
XXAPI void xrtNetFutureDestroy(xnetfuture* pFuture);
XXAPI xnet_result xrtNetFutureWait(xnetfuture* pFuture, uint32 iTimeoutMs);
XXAPI xnet_result xrtNetFutureStatus(xnetfuture* pFuture);
XXAPI ptr xrtNetFutureValue(xnetfuture* pFuture);
XXAPI xnetengine* xrtNetSyncGetHiddenEngine(void);
XXAPI void xrtNetSyncShutdownHiddenEngine(void);
```


## 15. Milestones and Progress Board

### M0. Freeze Legacy Stack

Exit gate:

- old network infrastructure preserved under `dev/net-v1/`
- freeze note added
- behavior reference tests identified

Tasks:

- [x] `M0-T01` Create `dev/net-v1/`
- [x] `M0-T02` Move or copy current network infrastructure into `dev/net-v1/`
- [x] `M0-T03` Preserve tests and sample references for legacy behavior
- [x] `M0-T04` Record freeze date and commit/tag reference

### M1. Base Types and Memory Core

Exit gate:

- base headers compile
- chain and block pool tests pass
- no hot-path memmove in the chain implementation

Tasks:

- [x] `M1-T01` Implement `xnet_base.h`
- [x] `M1-T02` Implement address parsing and formatting for IPv4 and IPv6
- [x] `M1-T03` Implement slab classes and dynamic large blocks
- [x] `M1-T04` Implement `xnetchain`
- [x] `M1-T05` Add unit tests for chain append, span, consume, ref-send

### M2. Backend Interface and Port Backends

Exit gate:

- abstract backend interface compiled
- Linux and Windows backends compile behind the same interface
- wakeup path and timer wake path exist

Tasks:

- [x] `M2-T01` Define internal `xnet_port` interface
- [x] `M2-T02` Implement IOCP backend skeleton
- [x] `M2-T03` Implement io_uring backend skeleton
- [x] `M2-T04` Implement worker wakeup mechanism
- [x] `M2-T05` Add backend smoke tests

### M3. Engine and Worker Runtime

Exit gate:

- engine starts and stops cleanly
- workers own their resources
- command posting works cross-thread
- timer wheel wakes callbacks correctly

Tasks:

- [x] `M3-T01` Implement engine lifecycle
- [x] `M3-T02` Implement worker threads
- [x] `M3-T03` Implement command queue
- [x] `M3-T04` Implement timer wheel
- [x] `M3-T05` Add engine runtime tests

### M4. Plain TCP Stream Transport

Exit gate:

- listener accepts connections
- stream connect works
- send queue, recv chain, and close path behave correctly
- backpressure callbacks work

Tasks:

- [x] `M4-T01` Implement listener object
- [x] `M4-T02` Implement stream object
- [x] `M4-T03` Implement recv chain delivery
- [x] `M4-T04` Implement send queue and gather send
- [x] `M4-T05` Implement pause/resume read
- [x] `M4-T06` Implement graceful and abort close paths
- [x] `M4-T07` Add TCP echo and slow-peer tests

### M5. UDP / Datagram Transport

Exit gate:

- UDP send and receive work
- source address semantics are correct
- no fake stream semantics leak into UDP

Tasks:

- [x] `M5-T01` Implement datagram socket object
- [x] `M5-T02` Implement packet receive path
- [x] `M5-T03` Implement packet send path
- [x] `M5-T04` Add UDP echo and burst tests

### M6. Builtin TLS Integration

Exit gate:

- stream can perform async TLS handshake
- plaintext protocol code is isolated from TLS record buffers
- TLS send and receive integrate with transport queues

Tasks:

- [x] `M6-T01` Define TLS stream adapter interface
- [x] `M6-T02` Attach builtin TLS session to stream objects
- [x] `M6-T03` Implement async handshake driving
- [x] `M6-T04` Integrate TLS send path with transport send queue
- [x] `M6-T05` Integrate TLS receive path with chain consumer model
- [x] `M6-T06` Add TLS echo and handshake tests

### M7. Sync Core

Exit gate:

- future and waiter model works
- sync facades no longer use direct socket loops
- one hidden engine path exists for convenience APIs

Tasks:

- [x] `M7-T01` Implement future object
- [x] `M7-T02` Implement wait helpers
- [x] `M7-T03` Implement hidden convenience engine policy
- [x] `M7-T04` Add sync facade tests

### M8. Protocol Rebuild Scaffolding

Exit gate:

- protocol code can be built without raw socket access
- codec layer is ready for HTTP and WS

Tasks:

- [x] `M8-T01` Define codec parser adapter interface
- [x] `M8-T02` Implement line and length-field codecs
- [x] `M8-T03` Implement HTTP1 parser skeleton
- [x] `M8-T04` Implement WebSocket frame skeleton

### M9. HTTP Client Skeleton

Exit gate:

- request, response, URL, and header objects exist
- one-shot async HTTP/1.1 client works over plain TCP and builtin TLS
- sync facade reuses the async core through `xnet_sync`
- Windows stage harness covers loopback HTTP and HTTPS cases
- completion and cleanup paths do not depend on polling future state after completion

Tasks:

- [x] `M9-T01` Define HTTP request, response, URL, and header objects
- [x] `M9-T02` Implement request serialization and response materialization helpers
- [x] `M9-T03` Implement one-shot async HTTP/1.1 client transaction on `xnet_stream`
- [x] `M9-T04` Implement sync HTTP facade on `xnet_sync`
- [x] `M9-T05` Add loopback HTTP and HTTPS regression tests
- [x] `M9-T06` Harden completion and cleanup semantics for sync shutdown safety

### M10. HTTP Server Skeleton

Exit gate:

- server wrapper owns listener lifecycle on top of `xnet_stream`
- request and response objects materialize from `xcodec_http1`
- plain HTTP and builtin TLS loopback paths work with the new `xhttp2` client
- accepted connection cleanup does not leak streams on close or server stop
- Windows stage harness covers plain and HTTPS server regressions

Tasks:

- [x] `M10-T01` Define server config plus request and response objects
- [x] `M10-T02` Implement listener wrapper and accept-thread skeleton
- [x] `M10-T03` Implement request parse and response serialization helpers
- [x] `M10-T04` Implement plain HTTP request handling path
- [x] `M10-T05` Implement builtin TLS request handling path
- [x] `M10-T06` Harden connection cleanup and response send ordering

### M11. WebSocket Skeleton

Exit gate:

- async WebSocket client and server wrappers exist on top of `xnet_stream`
- plain `ws://` and builtin-TLS `wss://` loopback paths work
- single-frame text and binary messaging works in both directions
- ping/pong and close control frames are handled without raw socket access
- Windows stage harness covers plain and TLS WebSocket regressions

Tasks:

- [x] `M11-T01` Define WebSocket client and server config plus event objects
- [x] `M11-T02` Implement HTTP upgrade handshake for client and server wrappers
- [x] `M11-T03` Implement single-frame text and binary send/receive helpers
- [x] `M11-T04` Implement ping/pong and close control handling
- [x] `M11-T05` Implement builtin TLS client and server handshake path
- [x] `M11-T06` Add plain and TLS loopback regression tests

### M12. Benchmark Scaffolding

Exit gate:

- `dev/bench/xnet2/` contains the planned benchmark entry points
- benchmark programs build against `xnet-v2` plus `xrt.c` in the Windows staging environment
- small smoke runs prove each benchmark can execute and emit metrics
- benchmark helpers stay dev-only and do not expand the public library API surface

Tasks:

- [x] `M12-T01` Add shared benchmark timing and argument helpers
- [x] `M12-T02` Add a reusable dev-only stream server helper for benchmark harnesses
- [x] `M12-T03` Implement idle TCP connection benchmark entry point
- [x] `M12-T04` Implement TCP and TLS echo benchmark entry points
- [x] `M12-T05` Implement UDP burst and send-queue pressure benchmark entry points
- [x] `M12-T06` Build and smoke-run benchmark binaries in the Windows staging environment

### M13. WebSocket Fragmentation and Message Reassembly

Exit gate:

- `xws2` reassembles fragmented text and binary messages on both client and server paths
- control frames remain legal while a fragmented message is in progress
- oversized fragmented messages close with the correct WebSocket close code
- partial-message buffers are released on close, stop, and cleanup paths
- Windows stage harness covers fragmented `ws://` and `wss://` regressions

Tasks:

- [x] `M13-T01` Add per-endpoint fragmented-message reassembly state to `xws2`
- [x] `M13-T02` Implement client-side fragmented text and binary consume paths
- [x] `M13-T03` Implement server-side fragmented text and binary consume paths
- [x] `M13-T04` Preserve ping/pong behavior and cleanup semantics during reassembly
- [x] `M13-T05` Add plain and TLS fragmented-message regressions

### M14. HTTP/1.1 Keep-Alive Reuse

Exit gate:

- `xhttpd2` can process multiple serial requests on one keep-alive connection
- response drain and close semantics remain owned by the same `xnet_stream`
- buffered bytes already present in the receive chain may be parsed after response drain
- Windows stage harness verifies same-socket keep-alive reuse with multiple requests
- client-side connection pooling remains deferred outside this milestone

Tasks:

- [x] `M14-T01` Track per-connection response-in-flight and keep-alive state in `xhttpd2`
- [x] `M14-T02` Re-enable request parsing after response drain on kept-alive connections
- [x] `M14-T03` Re-enter parsing when buffered next-request bytes are already present
- [x] `M14-T04` Add a raw loopback keep-alive reuse regression

### M15. HTTP Client Keep-Alive Reuse

Exit gate:

- `xhttp2` can reuse one idle keep-alive connection for serial requests to the same origin
- reuse works for both caller-owned engines and the hidden sync engine
- reused requests do not create a second accepted server connection in the loopback harness
- idle kept-alive connections can be explicitly purged before engine shutdown
- Windows stage harness verifies two serial client requests over one accepted connection

Tasks:

- [x] `M15-T01` Introduce an internal reusable client-connection object for `xhttp2`
- [x] `M15-T02` Reassign per-request ownership from `xhttp2` transactions to reusable connections
- [x] `M15-T03` Return completed keep-alive connections to an idle pool keyed by engine plus origin
- [x] `M15-T04` Add explicit idle-connection purge helpers for engine shutdown safety
- [x] `M15-T05` Add loopback regression coverage for serial keep-alive client reuse

### M16. HTTP Chunked Transfer

Exit gate:

- `xcodec_http1` can delimit whole chunked HTTP/1.1 messages and expose decoded body helpers
- `xhttp2` can serialize chunked requests and decode chunked responses over plain TCP and builtin TLS
- chunked keep-alive responses remain reusable when the full message has been delimited and consumed
- `xhttpd2` can decode chunked requests and serialize chunked responses without owning raw socket logic
- Windows stage harness covers codec, client, and server chunked regressions

Tasks:

- [x] `M16-T01` Extend `xcodec_http1` with whole-message chunked delimiting and dechunk helpers
- [x] `M16-T02` Teach `xhttp2` request serialization and response materialization about chunked transfer
- [x] `M16-T03` Teach `xhttpd2` request parse and response serialization paths about chunked transfer
- [x] `M16-T04` Add codec, client, and server chunked regression coverage and rerun the Windows stage harness


## 16. Test and Benchmark Plan

### 16.1 Unit Tests

- `test/test_xnet2_base.h`
- `test/test_xnet2_mem.h`
- `test/test_xnet2_engine.h`
- `test/test_xnet2_stream.h`
- `test/test_xnet2_dgram.h`
- `test/test_xnet2_tls.h`
- `test/test_xnet2_sync.h`
- `test/test_xnet2_codec.h`
- `test/test_xnet2_http2.h`
- `test/test_xnet2_httpd2.h`
- `test/test_xnet2_ws2.h`

### 16.2 Integration Tests

- TCP echo
- TCP half-close or close edge cases
- slow sender and slow receiver
- many idle connections
- UDP echo
- UDP burst traffic
- TLS handshake and echo
- sync future wait under timeout and cancellation
- one-shot HTTP client over plain TCP and builtin TLS
- one-shot HTTP server loopback over plain TCP and builtin TLS
- whole-message HTTP chunked request and response loopback paths
- one-shot WebSocket loopback over plain TCP and builtin TLS

### 16.3 Benchmarks

Initial benchmark programs to add under `dev/bench/xnet2/`:

- `bench_idle_conn.c`
- `bench_echo_tcp.c`
- `bench_echo_tls.c`
- `bench_udp_burst.c`
- `bench_send_queue_pressure.c`

### 16.4 Required Metrics

- idle memory per connection
- active memory per connection
- TCP echo throughput
- TCP echo p50 / p95 / p99 latency
- TLS handshake rate
- UDP packet rate
- slow-peer degradation profile

### 16.5 Acceptance Direction

The exact final numbers will depend on hardware, but the rewrite is expected to
show clear improvements relative to `xnet-v1` in:

- idle connection scaling
- send-heavy throughput
- TLS pipeline efficiency
- codepath unification for sync and async protocols


## 17. Risks and Mitigations

| ID | Risk | Impact | Mitigation |
| --- | --- | --- | --- |
| `R1` | io_uring feature variance across kernels | high | keep a backend capability table and single-shot fallback |
| `R2` | IOCP accept and worker dispatch complexity | high | keep port layer narrow and test accept ownership hard |
| `R3` | builtin TLS integration causes duplicate buffering | high | define TLS adapter around chains and queues early |
| `R4` | zero-copy send leaks buffer lifetimes | high | use explicit release callbacks and refcounts |
| `R5` | sync facade starts reintroducing a second transport path | high | forbid raw socket use in protocol sync code |
| `R6` | worker-local caches cause memory spikes | medium | add cache caps and trim points |
| `R7` | backpressure semantics are inconsistent across protocols | medium | define stream-level water rules once and reuse |
| `R8` | RTOS portability gets blocked by desktop assumptions | medium | keep `xnet_port` clean and avoid backend leakage |


## 18. Open Questions

- [ ] `Q1` Should Linux use per-worker `SO_REUSEPORT` listeners by default?
- [ ] `Q2` Should phase 1 support half-close, or defer it until stream core is stable?
- [x] `Q3` The hidden sync engine is process-global and one-worker by default in M7.
- [ ] `Q4` Should DNS stay blocking at first, or get an async resolver interface early?
- [x] `Q5` IPv6 is mandatory in M1 and is validated in `xnet_base` before M2 work begins.


## 19. Decision Log

Use this section for durable architecture decisions.

- `D-001` `2026-03-12`: `xnet-v2` is a full rewrite, not an in-place evolution of
  the current network stack.
- `D-002` `2026-03-12`: Builtin TLS remains the only supported TLS provider for
  the rewrite target architecture.
- `D-003` `2026-03-12`: Sync protocol APIs must be implemented as facades over
  the async core and may not own a second transport implementation.
- `D-004` `2026-03-12`: The initial public API target includes engine, stream,
  datagram, memory-chain, and future primitives before protocol rebuild.
- `D-005` `2026-03-12`: `net-v1` freeze is copy-based under `dev/net-v1/` so the
  live tree can keep building while `xnet-v2` is introduced incrementally.
- `D-006` `2026-03-12`: Phase-1 `xnet_mem` models zero-copy send lifetimes as
  explicit reference blocks with release callbacks instead of copying caller
  payloads into slab-managed blocks.
- `D-007` `2026-03-12`: Until `xrt.h` migration starts, `xnet-v2` staging headers
  are self-contained and are validated through the dedicated harness
  `dev/test_xnet2_stage.c` instead of the legacy omnibus `test.c` path.
- `D-008` `2026-03-12`: IPv6 support remains mandatory in M1, so `xnet_base`
  must ship widened address parsing, formatting, and resolution before M2.
- `D-009` `2026-03-12`: Non-native backend headers expose `NULL` ops on
  unsupported platforms so the dedicated xnet2 harness can include all backend
  headers in one translation unit without faking availability.
- `D-010` `2026-03-12`: The first `xnet_engine` skeleton owns worker-local
  `xnet_port` and `xnet_mem` contexts before worker threads exist, so M3 can
  validate lifecycle and ownership separately from threading.
- `D-011` `2026-03-12`: Cross-thread engine work is routed through a per-worker
  lock-protected command queue and woken through `xnet_port`, keeping the
  worker loop and backend wakeup path aligned before stream transport exists.
- `D-012` `2026-03-12`: The first worker timer API is expressed as delayed task
  posting on engine affinity keys, so timer wheel behavior can be validated on
  real worker threads before stream or datagram timers exist.
- `D-013` `2026-03-12`: `xnet_stream` staging validates listener/stream object
  ownership, local send-queue watermark transitions, and deferred graceful
  close through an internal queue-consume helper before backend socket I/O is
  wired into M4.
- `D-014` `2026-03-13`: Before backend socket ops land, `xnet_stream` stages
  transport progress through owner-worker posted send/recv helpers, buffered
  pause/resume dispatch, and gather-write preparation so queue, callback, and
  backpressure semantics can be validated independently of IOCP/io_uring
  completion wiring.
- `D-015` `2026-03-13`: Phase-1 backend skeletons now synthesize completion
  events for submitted send/recv-style ops, and `xnet_stream` consumes those
  events through the engine worker harvest path before raw socket handles are
  wired to real IOCP/io_uring submissions.
- `D-016` `2026-03-13`: `xnet_stream` now owns real nonblocking socket handles
  for listener start, client connect, accept-once staging, and abort/graceful
  close teardown, while send/recv byte transport remains on synthetic
  completion-backed staging until native backend I/O is attached.
- `D-017` `2026-03-13`: Before native IOCP/io_uring socket ops land,
  `xnet_stream` may advance real connected sockets through owner-worker send
  flushes and explicit socket-recv pump tasks so loopback byte transport and
  echo semantics can be validated without hard-coding the final backend
  submission shape too early.
- `D-018` `2026-03-13`: The phase-1 port backends now support one-shot socket
  watch submissions for `ACCEPT/RECV/SEND` when `iOpId != 0`, allowing staged
  readiness-driven harvest events to re-arm stream receive and send progress
  without committing yet to the final native IOCP or io_uring submission model.
- `D-019` `2026-03-13`: `xnet_engine` now exposes two transport port-event
  dispatch slots so `xnet_stream` and `xnet_dgram` can coexist on one engine
  during the staged rewrite without adding a heavier transport registry before
  TLS and protocol layers arrive.
- `D-020` `2026-03-13`: `xnet_tls` is implemented as a thin adapter over the
  in-tree TLS core, and the adapter explicitly bootstraps `xrtInit()` so the
  staged `xnet-v2` harness can reuse builtin TLS/crypto state without pulling
  legacy `xrt.h` networking types into the new transport headers.
- `D-021` `2026-03-13`: TLS handshake driving on staged stream backends must
  continue while either socket bytes remain readable or the builtin TLS context
  still has buffered ciphertext in its internal recv buffer; otherwise one-shot
  readiness watches can stall on multi-record server or client flights.
- `D-022` `2026-03-13`: `xnet_sync` uses a process-global hidden convenience
  engine with one worker by default, keeping sync-facade background cost low
  while still allowing advanced callers to provide their own engine.
- `D-023` `2026-03-13`: `xnet_sync` futures report pending as `XRT_NET_AGAIN`,
  resolve exactly once, and preserve the completion result across timed-out
  waits so sync facades can retry waiting without losing state.
- `D-024` `2026-03-13`: `xcodec` parsers operate only on `xnetchain` inputs and
  return frame metadata plus `NEED_MORE/FRAME/ERROR` status, so protocol code
  can be rebuilt without direct socket access or duplicate transport buffering.
- `D-025` `2026-03-13`: The first HTTP/1 codec stops at start-line plus headers
  and resolves fixed-length bodies when `Content-Length` is present, but leaves
  chunked-body decoding to later protocol work instead of embedding a half-built
  HTTP message state machine into the stage parser layer.
- `D-026` `2026-03-13`: The first WebSocket codec parses individual frame
  headers, payload boundaries, and mask metadata, while message reassembly and
  higher-level semantics remain the responsibility of the future `xws2` layer.
- `D-027` `2026-03-13`: `xhttp2` completion state is owned by the transaction
  rather than inferred from `xnetfuture` status so close and timeout callbacks
  do not touch a future after the request has already completed.
- `D-028` `2026-03-13`: `xhttp2` cleanup may discard request-body and send
  buffers as soon as stream teardown is scheduled, even if a timeout reference
  keeps the lightweight transaction shell alive until timer expiry or engine
  shutdown.
- `D-029` `2026-03-13`: `xhttpd2` must serialize and enqueue responses on the
  owner worker directly instead of calling socket-backed `xrtNetStreamSend()`
  from inside `OnRecv`, because the public send path is a posted operation and
  can be dropped if the stream is marked closing before the post is drained.
- `D-030` `2026-03-13`: until a real keep-alive manager exists, `xhttpd2`
  responds with `Connection: keep-alive` and does not proactively close the
  stream when the request advertises keep-alive; the client side is currently
  responsible for closing the one-shot loopback test connection after parsing
  the response.
- `D-031` `2026-03-13`: the first `xws2` layer owns the HTTP upgrade handshake
  itself on top of `xnet_stream` instead of routing through `xhttp2` or
  `xhttpd2`, because upgraded connections must retain transport ownership after
  the `101 Switching Protocols` response instead of collapsing back into a
  one-shot HTTP transaction model.
- `D-032` `2026-03-13`: phase-3 `xws2` deliberately supports only single-frame
  text and binary messages plus ping/pong and close control, and rejects
  continuation or fragmented message flows until a later milestone introduces
  explicit message-reassembly state on top of `xcodec_ws`.
- `D-033` `2026-03-13`: `dev/bench/xnet2/` may use benchmark-local wrappers
  around staging internals such as the manual accept-thread pattern instead of
  introducing new public benchmark-only server APIs into `lib/`.
- `D-034` `2026-03-13`: fragmented WebSocket message reassembly lives in
  `xws2` rather than `xcodec_ws`, so the codec stays frame-oriented while
  client/server wrappers own message lifetime, close semantics, and control
  frame interleaving rules.
- `D-035` `2026-03-13`: the first keep-alive upgrade for `xhttpd2` is
  deliberately serial and drain-driven; a connection becomes parseable again
  only after the previous response queue drains, which keeps ownership inside
  one `xnet_stream` without introducing a premature global pooling layer.
- `D-036` `2026-03-13`: `xhttp2` now owns a serial idle keep-alive pool keyed
  by engine plus origin plus TLS verification mode; idle connections remain
  outside active transactions and therefore require an explicit purge helper
  before engine shutdown until engine-level lifetime hooks exist.
- `D-037` `2026-03-13`: benchmark hardening must fail closed when the staged
  backend cannot complete a scenario credibly; on the current Windows staged
  backend, TLS echo is valid enough for smoke coverage, while plain TCP echo and
  send-queue pressure are still not trustworthy baseline measurements.
- `D-038` `2026-03-13`: first-wave HTTP chunked support is whole-message only:
  `xcodec_http1` owns message delimiting plus dechunk helpers, while `xhttp2`
  and `xhttpd2` consume decoded bodies and may serialize outbound bodies as a
  single chunked message without introducing streaming/trailer APIs yet.
- `D-039` `2026-03-13`: `xnet-v2` staging headers must carry their own minimal
  atomic helper layer instead of directly calling `__xrtAtomic*`, so the
  standalone harness remains self-contained while future `xrt.h` migration can
  still map onto the same semantics from above.
- `D-040` `2026-03-13`: until old networking public names are frozen or moved,
  full-xrt builds may use a narrow namespace bridge for the `xnet-v2` widened
  address model and conflicting helper symbols, while reusing the existing
  runtime's scalar types, socket type, result enum, and TLS config ABI.
- `D-041` `2026-03-13`: once a stream has entered graceful close, later socket
  faults on that same transport must still be allowed to escalate shutdown to
  abort and complete final close emission; reset races after shutdown has
  started must not be surfaced as fresh application errors.
- `D-042` `2026-03-13`: sync futures should expose a coroutine wait bridge when
  compiled inside full xrt runtime; the first version uses coroutine events to
  wake the owner scheduler, supports a single coroutine waiter per future, and
  intentionally preserves the existing thread-side condvar wait semantics.
- `D-043` `2026-03-13`: timeout-aware coroutine future wait should build on the
  coroutine timed-event path rather than reintroducing thread-style condvar
  polling; the first version exposes `xrtNetFutureWaitCoTimeout()` as an
  internal bridge and keeps the single coroutine-waiter limit.
- `D-044` `2026-03-13`: coroutine future wait should also support deadline-based
  waiting so xnet futures align with the coroutine runtime's
  sleep/deadline/event model; the first version adds `xrtNetFutureWaitCoUntil()`
  as an internal bridge and keeps timeout/infinite waits layered on top.
- `D-045` `2026-03-13`: coroutine integration should include a minimal
  `task/post -> future -> wake` bridge so worker tasks can complete back into
  coroutine schedulers without a second sync implementation; the first version
  adds `xrtNetEnginePostFuture()` and `xrtNetEnginePostDelayedFuture()` inside
  `xnet_sync.h`.
- `D-046` `2026-03-13`: posted futures must pin their own lifetime while a
  worker task is queued or running; `xrtNetFutureDestroy()` must reject
  destruction during that async-hold window so queued tasks cannot resolve into
  freed future storage.
- `D-047` `2026-03-13`: stream drain/close waits should reuse the future bridge
  instead of embedding coroutine state directly into the stream layer; the
  first version adds single-waiter drain/close registration slots in
  `xnet_stream.h`, exposes `xrtNetStreamDrainFuture()` and
  `xrtNetStreamCloseFuture()` from `xnet_sync.h`, and requires stream destroy to
  reject while an async wait hold still exists.
- `D-048` `2026-03-13`: readable stream waits must not race the existing
  callback-driven recv path; the first version therefore exposes
  `xrtNetStreamReadableFuture()` only for paused-read mode, backed by a single
  buffered-readable waiter slot in `xnet_stream.h`. Unpaused streams must reject
  readable-future registration instead of pretending to offer safe readiness
  semantics.
- `D-049` `2026-03-13`: writable stream waits should model backpressure relief
  instead of raw socket-ready state; the first version unifies stream waiters
  behind a shared wait-slot table in `xnet_stream.h` and exposes
  `xrtNetStreamWritableFuture()` as a one-shot future that resolves when queued
  send bytes fall from a high-water state to `low-water` or lower, or when the
  stream closes.
- `D-050` `2026-03-13`: once stream wait surfaces already resolve through
  one-shot futures, `xnet_sync.h` should also provide direct coroutine helpers
  so callers do not repeat `create future -> wait -> destroy`; the first
  version adds indefinite-only `xrtNetStreamWaitReadableCo()`,
  `xrtNetStreamWaitWritableCo()`, `xrtNetStreamWaitDrainCo()`, and
  `xrtNetStreamWaitCloseCo()`. Timed variants remain deferred until stream
  waiter cancellation/unregister semantics exist.
- `D-051` `2026-03-13`: timed direct stream coroutine waits must not return
  timeout/deadline while a waiter is still registered on the owner worker;
  `xnet_sync.h` therefore needs a worker-side unregister path for stream
  futures, and `xrtNetStreamWait*CoTimeout/Until()` may only complete after the
  pending waiter has been detached and its async-holds released.
- `D-052` `2026-03-13`: stream wait surfaces need a first generic abstraction
  layer before more readiness kinds are added; `xnet_sync.h` should therefore
  expose `xrtNetStreamFutureEx()` and `xrtNetStreamWaitCo*Ex()` keyed by a
  public stream wait-kind constant set, while the specialized readable/writable/
  drain/close helpers remain thin wrappers on top.
- `D-053` `2026-03-13`: the generic stream wait abstraction must serve both
  coroutine and thread callers; `xnet_sync.h` therefore also needs
  `xrtNetStreamWaitEx()/WaitTimeoutEx()/WaitUntilEx()` built on the same
  pending-wait cancellation path, so sync and coroutine waits share one
  lifecycle contract instead of drifting into separate implementations.
- `D-054` `2026-03-13`: before a broader socket/port wait-source model exists,
  `xnet_sync.h` may introduce a minimal `xnetwaitsrc` object that unifies
  existing `future` waits and `stream wait-kind` waits behind one common wait
  surface; the first version should route into the already-verified future and
  stream wait paths instead of inventing a second waiter lifecycle.
- `D-055` `2026-03-13`: the wait-source abstraction must also expose a value
  path, not only a status path; `xnet_sync.h` therefore needs
  `xrtNetWaitSourceWaitValue*()` and `xrtNetWaitSourceWaitCoValue*()` so future
  values and stream-event values can flow through one shared wait contract
  instead of forcing callers back into surface-specific getter logic.
- `D-056` `2026-03-13`: the next wait-source object after `future` and `stream`
  should be datagram receive, but it must stay contract-tight in phase-1:
  `xnet_sync.h` may expose `dgram recv` as a one-shot packet wait that returns
  a heap-owned `xnetdgrampkt`; the first version supports only a single waiter,
  rejects concurrent `OnRecv` callback usage, and does not promise datagram
  backlog buffering or multi-consumer fairness.
- `D-057` `2026-03-13`: `listener accept` must not be promoted into a wait-source
  until the listener itself owns a real async contract: owner-worker affinity,
  async-hold tracking, a single accept-wait slot or equivalent queue model, and
  accept registration/cancellation routed through `xnet_port`. A polling-loop
  wrapper around `accept()` is explicitly not acceptable as the `future` or
  coroutine-wait implementation path.


## 20. Session Log Template

Append one entry per work session.

Template:

```text
Date:
Author:
Scope:
Tasks touched:
Decisions made:
Code paths changed:
Tests run:
Benchmarks run:
Blockers:
Next steps:
```

Initial entry:

```text
Date: 2026-03-12
Author: Codex
Scope: Create the master rewrite spec for xnet-v2
Tasks touched: M0-T01, M1-T01..M8-T04 planning only
Decisions made: D-001..D-004
Code paths changed: none
Tests run: none
Benchmarks run: none
Blockers: Q1..Q5 unresolved
Next steps: freeze v1 into dev/net-v1 and start M0
```

Latest entry:

```text
Date: 2026-03-12
Author: Codex
Scope: Execute M0 and start M1-T01
Tasks touched: M0-T01, M0-T02, M0-T03, M0-T04, M1-T01
Decisions made: D-005
Code paths changed: added dev/net-v1 freeze snapshot, added lib/xnet_base.h staging header, updated spec progress
Tests run: none
Benchmarks run: none
Blockers: Q1..Q5 unresolved, xnet_base.h not yet integrated into xrt.h
Next steps: start M1-T02 and M1-T03, then define xnet_mem and the widened address model
```

Progress entry:

```text
Date: 2026-03-12
Author: Codex
Scope: Continue M1 with chain-backed memory model staging
Tasks touched: M1-T04
Decisions made: none
Code paths changed: added lib/xnet_mem.h with malloc-backed block-chain staging implementation
Tests run: none
Benchmarks run: none
Blockers: slab/cache path still pending in M1-T03, tests still pending in M1-T05
Next steps: implement allocator classes in xnet_mem and add test_xnet2_mem coverage
```

Progress entry:

```text
Date: 2026-03-12
Author: Codex
Scope: Add xnet2 memory test scaffold and validation notes
Tasks touched: M1-T05 (started)
Decisions made: none
Code paths changed: added test/test_xnet2_mem.h, included it in test.c in disabled state
Tests run: isolated syntax check for xnet_base.h, xnet_mem.h, and test_xnet2_mem.h passed
Benchmarks run: none
Blockers: full test.c syntax validation is currently blocked by pre-existing mismatches in test/test_xid.h and test/test_array_ptr.h
Next steps: implement slab/cache path, then wire xnet2 tests into a dedicated harness instead of the legacy omnibus test.c path
```

Progress entry:

```text
Date: 2026-03-12
Author: Codex
Scope: Close the M1 memory core staging work and validate ref-send chain semantics
Tasks touched: M1-T03, M1-T05
Decisions made: D-006
Code paths changed: xnet_mem gained cached slab classes, dynamic large blocks, reference blocks with release callbacks, and header guards; xnet_base gained a header guard; test_xnet2_mem coverage expanded to spans and ref-send behavior
Tests run: isolated syntax check for a minimal xrt.h + test/test_xnet2_mem.h harness passed; isolated dev/test_xnet2_mem.exe harness passed
Benchmarks run: none
Blockers: M1-T02 remains open because xnet_base staging still depends on the legacy address model; full test.c compilation is still blocked by pre-existing mismatches in test/test_xid.h and test/test_array_ptr.h
Next steps: implement the widened IPv4/IPv6 address model in xnet_base, then start the xnet_port and xnet_engine skeletons
```

Progress entry:

```text
Date: 2026-03-12
Author: Codex
Scope: Finish M1 by decoupling xnet2 staging from legacy xrt.h and implementing widened address support
Tasks touched: M1-T02
Decisions made: D-007, D-008
Code paths changed: rewrote lib/xnet_base.h as a self-contained IPv4/IPv6 staging header, switched lib/xnet_mem.h to self-contained allocation macros, added test/test_xnet2_base.h and dev/test_xnet2_stage.c, converted test/test_xnet2_mem.h to a standalone xnet2 test, and removed the xnet2 staging include from legacy test.c
Tests run: gcc -fsyntax-only dev/test_xnet2_stage.c -I.; gcc dev/test_xnet2_stage.c -I. -o dev/test_xnet2_stage.exe -lws2_32; ran dev/test_xnet2_stage.exe successfully
Benchmarks run: none
Blockers: none in M1; xnet2 remains intentionally detached from xrt.h until the later public integration phase
Next steps: start M2-T01 by defining the backend-neutral xnet_port interface, then build the engine/worker skeleton on top of it
```

Progress entry:

```text
Date: 2026-03-12
Author: Codex
Scope: Start M2 by defining the backend-neutral xnet_port contract
Tasks touched: M2-T01
Decisions made: none
Code paths changed: added lib/xnet_port.h with backend config, submit/event descriptors, vtable, and wrapper helpers; updated dev/test_xnet2_stage.c to compile the new port interface as part of the dedicated harness
Tests run: gcc -fsyntax-only dev/test_xnet2_stage.c -I.; gcc dev/test_xnet2_stage.c -I. -o dev/test_xnet2_stage.exe -lws2_32; ran dev/test_xnet2_stage.exe successfully
Benchmarks run: none
Blockers: M2-T02 and M2-T03 still need concrete backend skeletons; no wakeup path or timer backend exists yet
Next steps: implement the IOCP and io_uring backend skeletons behind lib/xnet_port.h, then add worker wakeup plumbing
```

Progress entry:

```text
Date: 2026-03-12
Author: Codex
Scope: Implement M2 backend skeletons and smoke-test wake/timer behavior
Tasks touched: M2-T02, M2-T03, M2-T04, M2-T05
Decisions made: D-009
Code paths changed: added lib/xnet_port_iocp.h and lib/xnet_port_uring.h skeleton backends, added test/test_xnet2_port.h backend smoke tests, and updated dev/test_xnet2_stage.c to exercise base, port, and memory staging layers together
Tests run: gcc -fsyntax-only dev/test_xnet2_stage.c -I.; gcc dev/test_xnet2_stage.c -I. -o dev/test_xnet2_stage.exe -lws2_32; ran .\\dev\\test_xnet2_stage.exe successfully on Windows
Benchmarks run: none
Blockers: native Linux compilation/runtime for the io_uring skeleton has not yet been exercised in this workspace; real accept/recv/send backend wiring is still pending later milestones
Next steps: start M3 by defining engine and worker skeletons that own xnet_port instances, then attach wakeup flow to the command queue
```

Progress entry:

```text
Date: 2026-03-12
Author: Codex
Scope: Start M3 with engine lifecycle and worker ownership bootstrap
Tasks touched: M3-T01
Decisions made: D-010
Code paths changed: added lib/xnet_engine.h with engine create/start/stop/destroy helpers, worker-local port bootstrap, and worker-local memory bootstrap; added test/test_xnet2_engine.h and exercised it via the dedicated xnet2 harness
Tests run: gcc -fsyntax-only dev/test_xnet2_stage.c -I.; gcc dev/test_xnet2_stage.c -I. -o dev/test_xnet2_stage.exe -lws2_32; ran .\\dev\\test_xnet2_stage.exe successfully on Windows; built and ran dev/test_xnet2_engine.exe successfully
Benchmarks run: none
Blockers: worker threads, command queue, and timer wheel are still missing; xrtNetEnginePost remains a stub until M3-T03
Next steps: implement worker threads and command queue so xrtNetEnginePost can dispatch work to owner workers
```

Progress entry:

```text
Date: 2026-03-12
Author: Codex
Scope: Implement worker threads, command queue, and engine runtime posting tests
Tasks touched: M3-T02, M3-T03, M3-T05
Decisions made: D-011
Code paths changed: expanded lib/xnet_engine.h with per-worker thread startup/shutdown, lock-protected command queues, worker loops, and a functional xrtNetEnginePost; expanded test/test_xnet2_engine.h to verify cross-thread posting and affinity routing
Tests run: gcc -fsyntax-only dev/test_xnet2_stage.c -I.; gcc dev/test_xnet2_stage.c -I. -o dev/test_xnet2_stage.exe -lws2_32; ran .\\dev\\test_xnet2_stage.exe successfully on Windows; built and ran dev/test_xnet2_engine.exe successfully
Benchmarks run: none
Blockers: timer wheel remains the only open item in M3; Linux-native thread/runtime validation is still pending outside this Windows workspace
Next steps: implement the timer wheel and attach timer callbacks to the worker loop so M3 can fully close before stream transport work begins
```

Progress entry:

```text
Date: 2026-03-12
Author: Codex
Scope: Close M3 by wiring a worker-local timer wheel into the engine runtime
Tasks touched: M3-T04
Decisions made: D-012
Code paths changed: extended lib/xnet_engine.h with a worker-local timer wheel, port timer pulse handling, delayed task posting, and timer-aware worker loops; expanded test/test_xnet2_engine.h to validate delayed execution and worker-affinity timer callbacks
Tests run: gcc -fsyntax-only dev/test_xnet2_stage.c -I.; gcc dev/test_xnet2_stage.c -I. -o dev/test_xnet2_stage.exe -lws2_32; ran .\\dev\\test_xnet2_stage.exe successfully on Windows; built and ran dev/test_xnet2_engine.exe successfully
Benchmarks run: none
Blockers: M3 is closed in the Windows staging environment; Linux-native runtime/timer validation is still pending outside this workspace
Next steps: start M4 and define the plain TCP stream/listener objects on top of the now-complete engine runtime
```

Progress entry:

```text
Date: 2026-03-12
Author: Codex
Scope: Start M4 with listener/stream objects and staged send-queue semantics
Tasks touched: M4-T01, M4-T02, M4-T05
Decisions made: D-013
Code paths changed: added lib/xnet_stream.h with listener and stream lifecycle helpers, worker assignment, local send-queue watermark tracking, graceful/abort close staging, and an internal send-queue consume helper; added test/test_xnet2_stream.h and updated dev/test_xnet2_stage.c to exercise the new stream layer
Tests run: gcc -fsyntax-only dev/test_xnet2_stage.c -I.; gcc dev/test_xnet2_stage.c -I. -o dev/test_xnet2_stage.exe -lws2_32; ran .\\dev\\test_xnet2_stage.exe successfully on Windows
Benchmarks run: none
Blockers: real accept/connect/recv/send backend submission is still pending in M4-T03, M4-T04, and M4-T06; Linux-native stream staging has not yet been exercised in this workspace
Next steps: wire recv/send backend ops into xnet_stream so listener accept, stream connect, and completion-driven queue draining can replace the current staged local helper path
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Extend M4 with owner-worker staged recv delivery and gather-send preparation
Tasks touched: M4-T03, M4-T04
Decisions made: D-014
Code paths changed: expanded lib/xnet_stream.h with accepted-stream staging, owner-worker posted send/recv helpers, buffered recv dispatch on resume, gather-write preparation, and write-complete queue consumption; rewrote test/test_xnet2_stream.h to validate accepted-stream inheritance, posted recv/send behavior, gather spans, and staged close/backpressure transitions
Tests run: gcc -fsyntax-only dev/test_xnet2_stage.c -I.; gcc dev/test_xnet2_stage.c -I. -o dev/test_xnet2_stage.exe -lws2_32; ran .\\dev\\test_xnet2_stage.exe successfully on Windows
Benchmarks run: none
Blockers: listener accept sockets, outbound connect, and backend-driven recv/send completions are still not wired to real IOCP/io_uring submissions; Linux-native stream staging has not yet been exercised in this workspace
Next steps: connect xnet_stream to xnet_port submissions so staged posted helpers can be replaced by real accept/connect/recv/send completion handlers
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Route staged stream transport through xnet_port completion harvest
Tasks touched: M2-T05, M4-T03, M4-T04
Decisions made: D-015
Code paths changed: expanded lib/xnet_engine.h with a pluggable port-event dispatcher, taught lib/xnet_port_iocp.h and lib/xnet_port_uring.h to synthesize completion events for submitted ops, and updated lib/xnet_stream.h so posted send/recv staging now flows through xnet_port submit/harvest before reaching stream callbacks and queue completion; refreshed test/test_xnet2_port.h and test/test_xnet2_stream.h to validate completion-driven submit and staged send/recv behavior
Tests run: gcc -fsyntax-only dev/test_xnet2_stage.c -I.; gcc dev/test_xnet2_stage.c -I. -o dev/test_xnet2_stage.exe -lws2_32; ran .\\dev\\test_xnet2_stage.exe successfully on Windows
Benchmarks run: none
Blockers: send/recv completions are still synthetic rather than backed by real socket operations; accept/connect/close still need native transport wiring and Linux-native validation remains pending outside this workspace
Next steps: replace synthetic submit completions with real listener accept, outbound connect, recv, send, and close wiring against IOCP/io_uring while preserving the now-stable port->engine->stream dispatch path
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Add real socket lifecycle for listener/connect/accept/close on top of the staged transport path
Tasks touched: M4-T01, M4-T02, M4-T06
Decisions made: D-016
Code paths changed: extended lib/xnet_base.h with a portable socket handle type and sockaddr conversion helper; expanded lib/xnet_stream.h with real listener bind/listen start, nonblocking client connect with timeout, internal accept-one staging, socket address refresh, open-event submission, and socket teardown on abort/graceful close; extended test/test_xnet2_stream.h with a loopback listener/connect/accept regression block
Tests run: gcc -fsyntax-only dev/test_xnet2_stage.c -I.; gcc dev/test_xnet2_stage.c -I. -o dev/test_xnet2_stage.exe -lws2_32; ran .\\dev\\test_xnet2_stage.exe successfully on Windows
Benchmarks run: none
Blockers: accepted/connect sockets are real now, but recv/send byte transport still rides synthetic backend completions instead of native IOCP/io_uring socket ops; listener accept is currently exposed through an internal accept-one helper rather than the final worker-owned backend path
Next steps: connect real socket recv/send/close and worker-owned accept polling/submission into xnet_port so TCP echo and slow-peer tests can close M4
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Add staged real socket byte pumps and a loopback echo regression on top of the new stream core
Tasks touched: M4-T04, M4-T07 (partial)
Decisions made: D-017
Code paths changed: expanded lib/xnet_stream.h so socket-backed public sends now post to the owner worker, real socket send queues flush directly on the owner worker, explicit socket-recv pump tasks can append into the recv chain, and resubmission logic stays aligned with the current staged transport split; expanded test/test_xnet2_stream.h so the loopback block now validates real client->server byte delivery and echoed bytes back to the client
Tests run: gcc -fsyntax-only dev/test_xnet2_stage.c -I.; gcc dev/test_xnet2_stage.c -I. -o dev/test_xnet2_stage.exe -lws2_32; ran .\\dev\\test_xnet2_stage.exe successfully on Windows
Benchmarks run: none
Blockers: loopback TCP echo is now covered, but slow-peer behavior is still missing and real recv/send readiness is still driven by explicit stage helpers instead of native IOCP/io_uring socket submissions
Next steps: replace the explicit socket-recv pump with backend-driven readiness/completion flow, then add slow-peer and queue-pressure regressions to close M4-T07
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Close M4 by shifting staged socket progress onto port-harvest-driven watches and adding slow-peer regression coverage
Tasks touched: M4-T03, M4-T04, M4-T07
Decisions made: D-018
Code paths changed: extended lib/xnet_stream.h with socket watch arm/disarm helpers, close finalization, readiness-driven recv/send re-arming, and auto-harvested loopback byte transport; expanded lib/xnet_port_iocp.h and lib/xnet_port_uring.h with one-shot socket watch registration/removal and harvest-driven readiness events; rewrote test/test_xnet2_stream.h loopback coverage to rely on automatic harvest and added a raw slow-peer regression that forces send-queue backpressure and later drain
Tests run: gcc -fsyntax-only dev/test_xnet2_stage.c -I.; gcc dev/test_xnet2_stage.c -I. -o dev/test_xnet2_stage.exe -lws2_32; ran .\\dev\\test_xnet2_stage.exe successfully on Windows
Benchmarks run: none
Blockers: M4 is closed in the Windows staging environment, but backend socket progress still uses staged readiness watches rather than final native IOCP or io_uring operations, and Linux-native validation remains pending outside this workspace
Next steps: start M5 by defining xnet_dgram and reuse the same owner-worker/chain/backpressure principles for UDP packet flow
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Implement and validate the phase-1 UDP/datagram transport
Tasks touched: M5-T01, M5-T02, M5-T03, M5-T04
Decisions made: D-019
Code paths changed: expanded lib/xnet_engine.h to support two transport port-event dispatch slots, taught lib/xnet_port_iocp.h and lib/xnet_port_uring.h to watch RECVFROM sockets, added lib/xnet_dgram.h with owner-worker UDP socket lifecycle, async send posting, readiness-driven recvfrom pumping, and source-address callbacks, added test/test_xnet2_dgram.h with UDP echo and burst coverage, and updated dev/test_xnet2_stage.c to execute the new datagram stage tests
Tests run: gcc -fsyntax-only dev/test_xnet2_stage.c -I.; gcc dev/test_xnet2_stage.c -I. -o dev/test_xnet2_stage.exe -lws2_32; ran .\\dev\\test_xnet2_stage.exe successfully on Windows
Benchmarks run: none
Blockers: M5 is closed in the Windows staging environment, but Linux-native validation is still pending outside this workspace and UDP batching is still a staged recvfrom loop rather than final native backend submission
Next steps: start M6 by defining the xnet_tls stream adapter surface and preserving the one-core rule while attaching builtin TLS to xnet_stream
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Deliver builtin TLS integration on top of xnet-v2 stream transport
Tasks touched: M6-T01, M6-T02, M6-T03, M6-T04, M6-T05, M6-T06
Decisions made: D-020, D-021
Code paths changed: added lib/xnet_tls.h as the builtin TLS adapter surface, extended lib/nettls.h with feed/pending/drain helpers for staged transport integration, wired lib/xnet_stream.h to attach TLS sessions on connect/accept, drive async handshakes on owner workers, route plaintext sends through TLS record generation, feed ciphertext receives into the builtin TLS context, and continue handshake progression while buffered TLS records remain, added test/test_xnet2_tls.h with loopback TLS handshake and echo coverage, updated dev/test_xnet2_stage.c to execute TLS stage tests, and added dev/xnet2_tls_test_cert.pem plus dev/xnet2_tls_test_key.pem as self-contained TLS fixtures for the Windows stage harness
Tests run: gcc -fsyntax-only dev/test_xnet2_stage.c -I.; gcc dev/test_xnet2_stage.c xrt.c -I. -o dev/test_xnet2_stage_tls.exe -lws2_32 -liphlpapi; launched dev/test_xnet2_stage_tls.exe via PowerShell Start-Process and verified exit code 0; confirmed stage output includes passing base, port, engine, stream, dgram, tls, and mem suites on Windows
Benchmarks run: none
Blockers: Linux-native validation is still pending outside this workspace, and builtin TLS handshake driving still reuses the legacy TLS state machine internally until the transport-decoupled TLS core is refactored later
Next steps: begin M7 by implementing xnet_sync futures and wait helpers on top of the now-working stream plus tls transport core
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Close M7 by adding xnet_sync futures, wait semantics, and the hidden convenience engine
Tasks touched: M7-T01, M7-T02, M7-T03, M7-T04
Decisions made: D-022, D-023
Code paths changed: added lib/xnet_sync.h with cross-platform future primitives, timed wait semantics, internal resolve/reset helpers, and a process-global one-worker hidden engine; added test/test_xnet2_sync.h with future timeout/resolve, engine-posted completion, and hidden-engine singleton coverage; updated dev/test_xnet2_stage.c to execute the sync stage tests
Tests run: gcc -fsyntax-only dev/test_xnet2_stage.c -I.; gcc dev/test_xnet2_stage.c xrt.c -I. -o dev/test_xnet2_stage_sync.exe -lws2_32 -liphlpapi; launched dev/test_xnet2_stage_sync.exe via PowerShell Start-Process and verified exit code 0; confirmed stage output includes passing base, port, engine, stream, dgram, tls, sync, and mem suites on Windows
Benchmarks run: none
Blockers: M7 is closed in the Windows staging environment, but Linux-native validation is still pending outside this workspace and no protocol-specific sync facades exist yet beyond the shared future/engine substrate
Next steps: begin M8 by defining the codec/parser adapter layer and line/length-field framing helpers on top of the now-complete phase-1 transport plus sync core
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Close M8 by adding codec adapters plus HTTP/1 and WebSocket parser skeletons
Tasks touched: M8-T01, M8-T02, M8-T03, M8-T04
Decisions made: D-024, D-025, D-026
Code paths changed: added lib/xcodec.h with the parser adapter contract, frame metadata helpers, and line plus length-field codecs; added lib/xcodec_http1.h with start-line/header parsing plus fixed-length body framing; added lib/xcodec_ws.h with frame-header, payload-boundary, and mask parsing; added test/test_xnet2_codec.h with line, length-field, HTTP/1 request/response, and WebSocket frame coverage; updated dev/test_xnet2_stage.c to execute the codec stage tests
Tests run: gcc -fsyntax-only dev/test_xnet2_stage.c -I.; gcc dev/test_xnet2_stage.c xrt.c -I. -o dev/test_xnet2_stage_codec.exe -lws2_32 -liphlpapi; launched dev/test_xnet2_stage_codec.exe via PowerShell Start-Process and verified exit code 0; confirmed stage output includes passing base, port, engine, stream, dgram, tls, sync, codec, and mem suites on Windows
Benchmarks run: none
Blockers: M8 is closed in the Windows staging environment, but Linux-native validation is still pending outside this workspace and the codec skeletons intentionally stop short of full chunked HTTP or multi-frame WebSocket message assembly
Next steps: begin upper-layer rebuild work by introducing xhttp2 request/response objects on top of xnet_stream, xnet_tls, xnet_sync, and xcodec_http1
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Deliver and harden the M9 HTTP client skeleton on top of xnet-v2
Tasks touched: M9-T01, M9-T02, M9-T03, M9-T04, M9-T05, M9-T06
Decisions made: D-027, D-028
Code paths changed: added lib/xhttp2.h with request/response objects, URL parsing, one-shot async HTTP/1.1 execution, and a sync facade over xnet_sync; added test/test_xnet2_http2.h with loopback HTTP and HTTPS regressions; updated dev/test_xnet2_stage.c to execute the HTTP client suite; added dev/test_xhttp2_only.c as a dedicated debug harness; and hardened xhttp2 completion, timeout, and cleanup paths so hidden-engine shutdown no longer depends on touching a completed future.
Tests run: gcc -fsyntax-only dev/test_xnet2_stage.c -I.; gcc dev/test_xnet2_stage.c xrt.c -I. -o dev/test_xnet2_stage_http2.exe -lws2_32 -liphlpapi; launched dev/test_xnet2_stage_http2.exe via PowerShell Start-Process and verified natural completion with passing base, port, engine, stream, dgram, tls, sync, codec, http2, and mem suites on Windows; gcc dev/test_xhttp2_only.c xrt.c -I. -o dev/test_xhttp2_only.exe -lws2_32 -liphlpapi; launched dev/test_xhttp2_only.exe via PowerShell Start-Process and verified natural completion with passing plain HTTP and HTTPS loopback cases.
Benchmarks run: none
Blockers: M9 is closed in the Windows staging environment, but chunked decoding, connection pooling or keep-alive reuse, and Linux-native validation remain pending outside this workspace.
Next steps: start the `xhttpd2` rebuild on top of xnet_stream, xnet_tls, and xcodec_http1 while preserving the one-core rule.
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Deliver the M10 HTTP server skeleton on top of xnet-v2
Tasks touched: M10-T01, M10-T02, M10-T03, M10-T04, M10-T05, M10-T06
Decisions made: D-029, D-030
Code paths changed: added lib/xhttpd2.h with server config, request/response objects, response serialization, accept-thread wrapper, per-connection cleanup flow, plain HTTP handling, builtin TLS handling, and owner-worker direct response enqueueing; added test/test_xnet2_httpd2.h with plain and HTTPS loopback regressions driven by the new xhttp2 client; updated dev/test_xnet2_stage.c to execute the HTTP server suite.
Tests run: gcc -fsyntax-only dev/test_xnet2_stage.c -I.; gcc dev/test_xnet2_stage.c xrt.c -I. -o dev/test_xnet2_stage_httpd2.exe -lws2_32 -liphlpapi; launched dev/test_xnet2_stage_httpd2.exe via PowerShell Start-Process and verified natural completion with passing base, port, engine, stream, dgram, tls, sync, codec, http2, httpd2, and mem suites on Windows.
Benchmarks run: none
Blockers: M10 is closed in the Windows staging environment, but request keep-alive reuse, chunked request decoding, static files, routing tables, and Linux-native validation remain pending outside this workspace.
Next steps: begin the `xws2` rebuild and/or deepen the HTTP stack with reusable server connection management instead of the current one-shot keep-alive compromise.
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Deliver the M11 WebSocket skeleton on top of xnet-v2
Tasks touched: M11-T01, M11-T02, M11-T03, M11-T04, M11-T05, M11-T06
Decisions made: D-031, D-032
Code paths changed: added lib/xws2.h with async WebSocket client/server wrappers, URL parsing, HTTP upgrade handshake, single-frame text/binary send helpers, automatic ping/pong handling, close control, and builtin TLS support; added test/test_xnet2_ws2.h with plain ws and wss loopback regressions covering open, text echo, binary echo, ping/pong, and close semantics; updated dev/test_xnet2_stage.c to execute the WebSocket suite.
Tests run: gcc -fsyntax-only dev/test_xnet2_stage.c -I.; gcc -fsyntax-only test/test_xnet2_ws2.h -I.; gcc dev/test_xnet2_stage.c xrt.c -I. -o dev/test_xnet2_stage_ws2.exe -lws2_32 -liphlpapi; ran dev/test_xnet2_stage_ws2.exe and verified exit code 0 with passing base, port, engine, stream, dgram, tls, sync, codec, http2, httpd2, ws2, and mem suites on Windows.
Benchmarks run: none
Blockers: M11 is closed in the Windows staging environment, but fragmented-message support, negotiated extensions, keep-alive-aware HTTP upgrade reuse, and Linux-native validation remain pending outside this workspace.
Next steps: either deepen `xws2` with fragmented message reassembly and higher-level convenience APIs, or turn back to the HTTP stack to replace the temporary one-shot keep-alive compromise with reusable connection management.
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Add benchmark scaffolding under dev/bench/xnet2 and validate it in the Windows staging environment
Tasks touched: M12-T01, M12-T02, M12-T03, M12-T04, M12-T05, M12-T06
Decisions made: D-033
Code paths changed: added dev/bench/xnet2/bench_common.h with timing, sleep, atomic, argument, and network-init helpers; added dev/bench/xnet2/bench_stream_server.h with a dev-only accepted-stream helper built on xnet_stream; added dev/bench/xnet2/bench_idle_conn.c, bench_echo_tcp.c, bench_echo_tls.c, bench_udp_burst.c, and bench_send_queue_pressure.c as initial benchmark entry points for idle TCP, TCP echo, TLS echo, UDP burst, and send-queue pressure scenarios.
Tests run: gcc -fsyntax-only dev/bench/xnet2/bench_idle_conn.c -I.; gcc -fsyntax-only dev/bench/xnet2/bench_echo_tcp.c -I.; gcc -fsyntax-only dev/bench/xnet2/bench_echo_tls.c -I.; gcc -fsyntax-only dev/bench/xnet2/bench_udp_burst.c -I.; gcc -fsyntax-only dev/bench/xnet2/bench_send_queue_pressure.c -I.; gcc dev/bench/xnet2/bench_idle_conn.c xrt.c -I. -o dev/bench/xnet2/bench_idle_conn.exe -lws2_32 -liphlpapi; gcc dev/bench/xnet2/bench_echo_tcp.c xrt.c -I. -o dev/bench/xnet2/bench_echo_tcp.exe -lws2_32 -liphlpapi; gcc dev/bench/xnet2/bench_echo_tls.c xrt.c -I. -o dev/bench/xnet2/bench_echo_tls.exe -lws2_32 -liphlpapi; gcc dev/bench/xnet2/bench_udp_burst.c xrt.c -I. -o dev/bench/xnet2/bench_udp_burst.exe -lws2_32 -liphlpapi; gcc dev/bench/xnet2/bench_send_queue_pressure.c xrt.c -I. -o dev/bench/xnet2/bench_send_queue_pressure.exe -lws2_32 -liphlpapi; executed small smoke runs for all five benchmark binaries and confirmed they emit metrics on Windows.
Benchmarks run: small smoke runs only, not representative performance measurements.
Blockers: M12 is closed for scaffolding, but Linux-native benchmark validation, realistic multi-connection throughput runs, and benchmark interpretation guides are still pending; current small UDP burst smoke can show packet loss under intentionally bursty sends, and send-queue pressure only hits watermarks once payload volume exceeds kernel socket buffering.
Next steps: run larger benchmark sweeps, record baseline numbers, and continue closing DoD gaps such as Linux-native validation plus HTTP keep-alive and connection-reuse work.
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Close WebSocket fragmented-message support and HTTP/1.1 server keep-alive reuse gaps in the Windows staging environment
Tasks touched: M13-T01, M13-T02, M13-T03, M13-T04, M13-T05, M14-T01, M14-T02, M14-T03, M14-T04
Decisions made: D-034, D-035
Code paths changed: updated lib/xws2.h with fragmented message reassembly state, client/server continuation handling, control-frame interleave handling, and cleanup of partial-message buffers; updated test/test_xnet2_ws2.h with plain ws and wss fragmented-message regressions; updated lib/xhttpd2.h with serial keep-alive response-in-flight gating and drain-driven request reentry on kept-alive streams; updated test/test_xnet2_httpd2.h with a raw same-socket keep-alive reuse regression.
Tests run: gcc -fsyntax-only test/test_xnet2_ws2.h -I.; gcc -fsyntax-only test/test_xnet2_httpd2.h -I.; gcc -fsyntax-only dev/test_xnet2_stage.c -I.; gcc dev/test_xnet2_stage.c xrt.c -I. -o dev/test_xnet2_stage_http_keepalive.exe -lws2_32 -liphlpapi; ran dev/test_xnet2_stage_http_keepalive.exe and verified exit code 0 with passing base, port, engine, stream, dgram, tls, sync, codec, http2, httpd2, ws2, and mem suites on Windows, including fragmented ws/wss and raw keep-alive HTTP loopback cases.
Benchmarks run: none
Blockers: Linux-native validation, client-side HTTP connection pooling/reuse, chunked HTTP decode, negotiated WebSocket extensions, and benchmark baseline capture remain pending outside this workspace.
Next steps: either add xhttp2-side connection pooling on top of the new keep-alive-capable server path, or run larger benchmark sweeps and Linux-native validation to close the remaining DoD gaps.
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Add serial keep-alive client reuse to xhttp2 and keep the stage harness aligned with the new behavior
Tasks touched: M15-T01, M15-T02, M15-T03, M15-T04, M15-T05
Decisions made: D-036, D-037
Code paths changed: updated lib/xhttp2.h so transactions lease reusable client connections instead of always owning one-shot streams, added an idle keep-alive pool keyed by engine plus origin plus TLS verification mode, added xrtHttp2CloseIdleConnections(...) for shutdown-safe idle-connection purge, and extended test/test_xnet2_http2.h plus test/test_xnet2_httpd2.h to validate serial client reuse and to explicitly purge idle pooled connections before asserting server-side close callbacks
Tests run: gcc -fsyntax-only lib/xhttp2.h -I.; gcc -fsyntax-only test/test_xnet2_http2.h -I.; gcc -fsyntax-only test/test_xnet2_httpd2.h -I.; gcc -fsyntax-only dev/test_xnet2_stage.c -I.; gcc dev/test_xhttp2_only.c xrt.c -I. -o dev/test_xhttp2_only.exe -lws2_32 -liphlpapi; ran dev/test_xhttp2_only.exe successfully; gcc dev/test_xnet2_stage.c xrt.c -I. -o dev/test_xnet2_stage_http_pool.exe -lws2_32 -liphlpapi; ran dev/test_xnet2_stage_http_pool.exe successfully with passing base, port, engine, stream, dgram, tls, sync, codec, http2, httpd2, ws2, and mem suites on Windows
Benchmarks run: reran dev/bench/xnet2/bench_echo_tcp.exe, dev/bench/xnet2/bench_echo_tls.exe, and dev/bench/xnet2/bench_send_queue_pressure.exe on Windows; TLS echo completed credibly, while plain TCP echo and send-queue pressure still fail closed and are not yet accepted as baseline data
Blockers: Linux-native validation, chunked HTTP decode, negotiated WebSocket extensions, trustworthy plain-TCP and queue-pressure benchmark baselines on the staged Windows backend, and final performance comparison against xnet-v1 remain open
Next steps: either implement chunked HTTP handling next, or shift to benchmark/native-backend work so the remaining DoD items become measurable instead of structural
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Close the HTTP chunked transfer gap across codec, client, and server layers
Tasks touched: M16-T01, M16-T02, M16-T03, M16-T04
Decisions made: D-038
Code paths changed: extended lib/xcodec_http1.h with whole-message chunked delimiting, decoded-body sizing/copy helpers, and chunked frame metadata; updated lib/xhttp2.h so requests may serialize with Transfer-Encoding: chunked, responses decode chunked bodies, and chunked keep-alive responses remain reusable; updated lib/xhttpd2.h so chunked requests materialize as decoded request bodies and chunked responses serialize without Content-Length; expanded test/test_xnet2_codec.h, test/test_xnet2_http2.h, and test/test_xnet2_httpd2.h with chunked codec/client/server regressions
Tests run: gcc -fsyntax-only lib/xcodec_http1.h -I.; gcc -fsyntax-only lib/xhttp2.h -I.; gcc -fsyntax-only lib/xhttpd2.h -I.; gcc -fsyntax-only test/test_xnet2_codec.h -I.; gcc -fsyntax-only test/test_xnet2_http2.h -I.; gcc -fsyntax-only test/test_xnet2_httpd2.h -I.; gcc dev/test_xhttp2_only.c xrt.c -I. -o dev/test_xhttp2_chunked.exe -lws2_32 -liphlpapi; ran dev/test_xhttp2_chunked.exe successfully; gcc dev/test_xnet2_stage.c xrt.c -I. -o dev/test_xnet2_stage_chunked.exe -lws2_32 -liphlpapi; ran dev/test_xnet2_stage_chunked.exe successfully with passing base, port, engine, stream, dgram, tls, sync, codec, http2, httpd2, ws2, and mem suites on Windows
Benchmarks run: none
Blockers: Linux-native validation, negotiated WebSocket extensions, trustworthy plain-TCP and queue-pressure benchmark baselines on the staged Windows backend, and final performance comparison against xnet-v1 remain open
Next steps: either move to Linux-native validation and benchmark-baseline work, or implement the remaining higher-level protocol conveniences such as negotiated WebSocket extensions
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Align xnet_engine worker threading with the new XRT runtime threading model without breaking the standalone xnet2 staging harness
Tasks touched: threading/runtime integration follow-up, xnet engine worker lifecycle
Decisions made: when xnet-v2 is compiled inside the full xrt runtime, worker threads should be created through xrtThreadCreate/xrtThreadWait/xrtThreadDestroy so they inherit attach/detach lifecycle and thread-local runtime state; when xnet-v2 is compiled in the standalone staging harness, the previous native CreateThread/pthread_create fallback remains in place until full xrt.h migration is complete
Code paths changed: updated lib/xnet_engine.h so worker thread storage uses a neutral pointer plus uint64 thread id, added __XNET_ENGINE_USE_XRT_THREAD gating, switched the integrated path to xrt-managed worker threads, and kept the standalone stage path self-contained; updated the worker ownership check to use xrtThreadGetCurrentId() when available; updated dev/XNET_V2_SPEC.md worker invariants to record the runtime-attach requirement
Tests run: gcc -m64 -c dev/tmp_xnet2_engine_stage_check.c -o tmp_xnet2_engine_stage_check.o; gcc -m64 dev/tmp_xnet2_engine_stage_check.c -o dev/tmp_xnet2_engine_stage_check.exe -lws2_32 -liphlpapi -lshell32; ran dev/tmp_xnet2_engine_stage_check.exe successfully and verified the engine skeleton suite passed on Windows
Benchmarks run: none
Blockers: full integrated xrt-mode execution is still blocked by the broader xnet2 staging headers not yet being fully migrated onto xrt.h/shared runtime primitives; test_xnet2_stage.c still carries separate standalone assumptions in xnet_sync and related headers
Next steps: either continue the xnet2-to-xrt runtime migration so integrated mode becomes directly runnable, or move to the next module while preserving this boundary in the spec
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Remove direct __xrtAtomic dependencies from xnet-v2 staging headers and restore standalone compilation of the sync/http/ws layer
Tasks touched: xnet base/runtime migration follow-up, sync/http/ws platform helper cleanup
Decisions made: D-039
Code paths changed: updated lib/xnet_base.h to provide local compare-exchange, exchange, add-fetch, and load helpers; switched lib/xnet_sync.h, lib/xhttp2.h, lib/xhttpd2.h, and lib/xws2.h to those helpers; simplified staged shared-lock unlock paths in xhttpd2/xws2 to use atomic exchange instead of compare-exchange-on-unlock
Tests run: gcc -fsyntax-only dev/test_xnet2_stage.c -I.; gcc dev/test_xnet2_stage.c xrt.c -I. -o dev/test_xnet2_stage_syncfix.exe -lws2_32 -liphlpapi; ran dev/test_xnet2_stage_syncfix.exe successfully on Windows and verified the stage suite completed
Benchmarks run: none
Blockers: direct one-translation-unit xrt-integration of xnet-v2 is still blocked by namespace/type collisions between legacy xrt networking types and the widened xnet-v2 base model, most notably xnetaddr/xtlsconfig/xnet_result
Next steps: decide whether the next migration step is a temporary namespace-bridge for xnet-v2 staging inside full xrt builds, or a broader old-network freeze that clears those public names for the v2 model
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Add a minimal full-xrt namespace bridge for xnet-v2 base types so integrated builds can move past typedef collisions
Tasks touched: xnet-v2 to xrt migration follow-up, legacy TLS ABI bridge cleanup
Decisions made: D-040
Code paths changed: updated lib/xnet_base.h to reuse xrt scalar/socket/result/TLS types under XXRTL_CORE while remapping the widened v2 address model to an internal xnet2_addr name and remapping the conflicting xrtNetResolve helper; updated lib/xnet_tls.h so full-xrt builds reuse the existing xtlsctx/xrtGlobalData/xtlsconfig definitions instead of redefining them inside the staging adapter
Tests run: gcc -fsyntax-only dev/test_xnet2_stage.c -I.; inline compile checks for xrt.c plus xnet-v2 headers in one translation unit
Benchmarks run: none
Blockers: wider integrated builds can now move beyond the old typedef layer, but remaining collisions and behavioral mismatches must still be surfaced module by module; old public xnetaddr and xnet-v2 widened address helpers still cannot coexist under the same external public name
Next steps: compile an integrated xrt + xnet-v2 translation unit again, capture the next concrete blocker after the typedef bridge, and decide whether to continue bridging or freeze the old network public surface sooner
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Fix integrated close-path regressions exposed by the full xnet-v2 WebSocket TLS suite under xrt-managed worker threads
Tasks touched: integrated xnet-v2 regression follow-up, stream close semantics
Decisions made: D-041
Code paths changed: updated lib/xnet_stream.h so a later abort request can escalate a stream that is already in graceful close, and suppressed duplicate OnError emission for non-wouldblock socket faults after shutdown has already started; this closes the gap where WS/TLS close races could leave a stream stuck in bClosing without a final close callback
Tests run: focused integrated WS2 TLS debug repro using xrt.c plus test/test_xnet2_ws2.h; full integrated xrt.c + all xnet2 test headers stage compile/run
Benchmarks run: none
Blockers: old public-name overlap remains a migration concern, but integrated runtime behavior is now much closer to standalone staging behavior
Next steps: rerun standalone and integrated xnet-v2 stage suites, then decide whether to formalize the bridge layer or continue moving selected xnet-v2 modules into the main xrt build
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Add the first coroutine-aware future wait bridge for full-xrt runtime builds
Tasks touched: xnet-sync future bridge, coroutine integration
Decisions made: D-042
Code paths changed: extended lib/xnet_sync.h so xnetfuture can lazily own a coroutine event plus a single coroutine-waiter slot under XXRTL_CORE; added xrtNetFutureWaitCo() as a core-only helper; updated future resolve/reset/destroy paths to cooperate with coroutine wait state; expanded test/test_xnet2_sync.h with a guarded coroutine-future wait regression
Tests run: dedicated core-mode compile/run harness using xrt.h + lib/xnet_sync.h + test/test_xnet2_sync.h on Windows
Benchmarks run: none
Blockers: full public xrt exposure for xnetfuture is still deferred until the wider xnet-v2 integration window; timeout-aware coroutine future wait is not implemented yet, only indefinite wait
Next steps: decide whether to add timeout/deadline-aware future wait on top of coroutine timed event support, or keep the bridge minimal and move on to the next xnet-v2 integration surface
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Extend the coroutine-aware future bridge with timeout semantics on top of timed coroutine events
Tasks touched: xnet-sync future bridge, coroutine integration follow-up
Decisions made: D-043
Code paths changed: updated lib/xnet_sync.h to add xrtNetFutureWaitCoTimeout(); reused xrtCoWaitEventTimeout() so unresolved futures can return XRT_NET_TIMEOUT without blocking the thread; expanded test/test_xnet2_sync.h with a guarded coroutine-future-timeout regression
Tests run: dedicated core-mode compile/run harness using xrt.h + lib/xnet_sync.h + test/test_xnet2_sync.h on Windows
Benchmarks run: none
Blockers: full public xrt exposure for xnetfuture is still deferred until the wider xnet-v2 integration window; coroutine future wait still supports only a single coroutine waiter and currently exposes timeout, not deadline-based waiting
Next steps: decide whether to add deadline-aware coroutine future wait, or move to the next xnet-v2 integration surface such as task/post driven coroutine wakeups
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Extend the coroutine-aware future bridge with deadline-based waiting
Tasks touched: xnet-sync future bridge, coroutine integration follow-up
Decisions made: D-044
Code paths changed: updated lib/xnet_sync.h to add xrtNetFutureWaitCoUntil(); layered xrtNetFutureWaitCo() and xrtNetFutureWaitCoTimeout() on top of the same deadline-aware coroutine event path; expanded test/test_xnet2_sync.h with a guarded coroutine-future-deadline regression
Tests run: dedicated core-mode compile/run harness using xrt.h + lib/xnet_sync.h + test/test_xnet2_sync.h on Windows
Benchmarks run: none
Blockers: full public xrt exposure for xnetfuture is still deferred until the wider xnet-v2 integration window; coroutine future wait still supports only a single coroutine waiter and currently offers indefinite/timeout/deadline waits, but not a richer wake-reason contract
Next steps: decide whether to make coroutine future wait public during the broader xnet-v2 merge window, or continue with the next xnet-v2 integration surface such as task/post driven coroutine wakeups
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Add the first task/post -> future -> coroutine wake bridge in xnet_sync
Tasks touched: xnet-sync future bridge, engine task integration, coroutine integration follow-up
Decisions made: D-045
Code paths changed: updated lib/xnet_sync.h to add xnet_future_task_fn plus xrtNetEnginePostFuture()/xrtNetEnginePostDelayedFuture(); worker tasks now auto-resolve a generated future that can be awaited by coroutines through the existing future-wait bridge; expanded test/test_xnet2_sync.h with guarded post-future and delayed-post-future coroutine regressions
Tests run: dedicated core-mode compile/run harness using xrt.h + lib/xnet_sync.h + test/test_xnet2_sync.h on Windows
Benchmarks run: none
Blockers: full public xrt exposure for xnetfuture and the new posted-future helpers is still deferred until the wider xnet-v2 integration window; the bridge currently models one-shot task completion only and still keeps the single coroutine waiter limit
Next steps: decide whether to expose the posted-future helpers more broadly during the xnet-v2 merge, or continue with the next integration surface such as socket/stream wait sources
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Harden posted-future lifetime so queued worker tasks cannot resolve into freed futures
Tasks touched: xnet-sync future bridge, engine task integration follow-up
Decisions made: D-046
Code paths changed: extended lib/xnet_sync.h so xnetfuture tracks an internal async-hold count; posted/delayed future creation now pins the future before queueing work and releases that hold only after the worker resolves the future; xrtNetFutureDestroy() now rejects destruction while async-hold is still present; expanded test/test_xnet2_sync.h with a guarded regression that rejects early destroy, verifies the queued task still resolves, and then allows final destroy
Tests run: dedicated core-mode compile/run harness using xrt.h + lib/xnet_sync.h + test/test_xnet2_sync.h on Windows
Benchmarks run: none
Blockers: full public xrt exposure for xnetfuture and posted-future helpers is still deferred until the wider xnet-v2 integration window; the bridge still models one-shot task completion only and keeps the single coroutine waiter limit
Next steps: continue with the next integration surface such as stream/socket wait sources, now that posted-future lifetime is explicitly hardened
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Add the first stream wait-source bridge so drain/close events can wake coroutine waiters through futures
Tasks touched: xnet-sync future bridge, stream lifecycle integration, coroutine integration follow-up
Decisions made: D-047
Code paths changed: updated lib/xnet_stream.h with single-waiter sync drain/close slots, stream async-hold tracking, and destroy rejection while async waiters still hold the stream; updated lib/xnet_sync.h to add xrtNetStreamDrainFuture() and xrtNetStreamCloseFuture(), which register waiters on the owner worker and resolve futures on drain/close; expanded test/test_xnet2_sync.h with guarded coroutine regressions for stream-drain wait, stream-close wait, and early stream-destroy rejection while a stream future is active
Tests run: gcc -m64 -c xrt.c -I.; gcc -m64 -c test.c -I.; gcc -fsyntax-only dev/test_xnet2_stage.c -I.; dedicated core-mode compile/run harness using xrt.h + lib/xnet_sync.h + test/test_xnet2_sync.h on Windows
Benchmarks run: none
Blockers: the bridge currently supports only one drain waiter and one close waiter per stream, and still resolves through one-shot futures rather than a richer reusable wait-source abstraction
Next steps: decide whether to generalize the stream wait registration model into a reusable socket/stream wait-source table, or continue by bridging the next concrete xnet surface such as readable/writable readiness
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Add the first readable stream future bridge on top of paused-read buffered recv state
Tasks touched: xnet-sync future bridge, stream recv lifecycle integration, coroutine integration follow-up
Decisions made: D-048
Code paths changed: updated lib/xnet_stream.h with a single readable waiter slot plus readable notifications on recv-buffer append, recv-chain splice, and close paths; updated lib/xnet_sync.h to add xrtNetStreamReadableFuture(), which currently requires paused-read mode and resolves when buffered recv data becomes available or the stream closes; expanded test/test_xnet2_sync.h with guarded regressions for readable future wait and explicit rejection on unpaused streams
Tests run: gcc -m64 -c xrt.c -I.; gcc -m64 -c test.c -I.; gcc -fsyntax-only dev/test_xnet2_stage.c -I.; dedicated core-mode compile/run harness using xrt.h + lib/xnet_sync.h + test/test_xnet2_sync.h on Windows
Benchmarks run: none
Blockers: readable future currently supports only one waiter and only the paused-read buffered contract; it is not yet a general-purpose readiness API for callback-driven streams
Next steps: decide whether to keep expanding the future bridge with writable/readiness helpers, or collapse the current drain/close/readable contracts into a reusable wait-source table before adding more surfaces
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Add the first writable stream future bridge on top of unified stream wait slots
Tasks touched: xnet-sync future bridge, stream send/backpressure lifecycle integration, coroutine integration follow-up
Decisions made: D-049
Code paths changed: updated lib/xnet_stream.h so readable/writable/drain/close waiters all share one wait-slot table and writable wait resolves when send pressure drops from high-water to low-water or below; updated lib/xnet_sync.h to add xrtNetStreamWritableFuture(); expanded test/test_xnet2_sync.h with guarded regressions for writable future wait, early stream-destroy rejection while writable wait is active, and low-water relief semantics
Tests run: gcc -m64 -c xrt.c -I.; gcc -m64 -c test.c -I.; gcc -fsyntax-only dev/test_xnet2_stage.c -I.; dedicated core-mode compile/run harness using xrt.h + lib/xnet_sync.h + test/test_xnet2_sync.h on Windows
Benchmarks run: none
Blockers: writable future still supports only one waiter and models queue-pressure relief rather than a richer callback-safe readiness API
Next steps: continue collapsing stream/socket wait surfaces toward a reusable wait-source abstraction before exposing more readiness helpers
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Add direct coroutine wait helpers on top of the existing stream future bridge
Tasks touched: xnet-sync future bridge, coroutine integration usability layer
Decisions made: D-050
Code paths changed: updated lib/xnet_sync.h to add indefinite-only xrtNetStreamWaitReadableCo()/WaitWritableCo()/WaitDrainCo()/WaitCloseCo() wrappers that internally create the matching stream future, wait in coroutine context, and destroy the future; expanded test/test_xnet2_sync.h with guarded regressions for direct readable and writable coroutine helpers
Tests run: gcc -m64 -c xrt.c -I.; gcc -m64 -c test.c -I.; gcc -fsyntax-only dev/test_xnet2_stage.c -I.; dedicated core-mode compile/run harness using xrt.h + lib/xnet_sync.h + test/test_xnet2_sync.h on Windows
Benchmarks run: none
Blockers: direct helpers are intentionally indefinite-only for now because stream waiters still lack a timeout/deadline cancellation protocol
Next steps: continue collapsing stream/socket wait surfaces toward a reusable wait-source abstraction, then revisit timed direct helpers once waiter unregister semantics exist
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Add timeout/deadline direct coroutine waits on top of the existing stream future bridge
Tasks touched: xnet-sync future bridge, stream waiter lifecycle hardening, coroutine integration usability layer
Decisions made: D-051
Code paths changed: updated lib/xnet_stream.h to add __xnetStreamCancelSyncWait(); extended lib/xnet_sync.h so stream future wait contexts carry a small state machine, a cancellation-complete event, and a pending-cleanup hook owned by xnetfuture; added xrtNetStreamWait*CoTimeout() and xrtNetStreamWait*CoUntil(); expanded test/test_xnet2_sync.h with guarded regressions that time out readable/writable direct waits and then successfully re-register the same surface
Tests run: gcc -m64 -c xrt.c -I.; gcc -m64 -c test.c -I.; gcc -fsyntax-only dev/test_xnet2_stage.c -I.; dedicated core-mode compile/run harness using xrt.h + lib/xnet_sync.h + test/test_xnet2_sync.h on Windows
Benchmarks run: none
Blockers: direct timed waits are now implemented for existing stream wait surfaces, but the broader reusable wait-source abstraction and additional socket-level readiness contracts are still deferred
Next steps: continue collapsing stream/socket wait surfaces toward a reusable wait-source abstraction, now that timed direct coroutine waits no longer leave dangling stream waiters
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Complete timed direct coroutine wait coverage for drain/close stream surfaces
Tasks touched: xnet-sync future bridge tests, coroutine integration regression coverage
Decisions made: D-051
Code paths changed: expanded test/test_xnet2_sync.h with guarded regressions for stream-drain timeout helper and stream-close deadline helper; both tests first time out, then re-register the same stream wait surface and verify the retried wait still resolves correctly
Tests run: gcc -m64 -c xrt.c -I.; gcc -m64 -c test.c -I.; gcc -fsyntax-only dev/test_xnet2_stage.c -I.; dedicated core-mode compile/run harness using xrt.h + lib/xnet_sync.h + test/test_xnet2_sync.h on Windows
Benchmarks run: none
Blockers: stream wait surfaces now have timed direct-helper coverage, but the broader reusable wait-source abstraction and additional socket-level readiness contracts are still deferred
Next steps: continue collapsing stream/socket wait surfaces toward a reusable wait-source abstraction, with the timed direct helper contract now fully covered for readable/writable/drain/close
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Add the first generic stream wait abstraction layer above the existing specialized helpers
Tasks touched: xnet-sync future bridge, stream wait-source API shaping, coroutine integration usability layer
Decisions made: D-052
Code paths changed: updated lib/xnet_stream.h to expose public wait-kind constants; updated lib/xnet_sync.h so stream future registration is table-driven by wait-kind and now exposes xrtNetStreamFutureEx() plus xrtNetStreamWaitCoEx()/WaitCoTimeoutEx()/WaitCoUntilEx(); expanded test/test_xnet2_sync.h with guarded regressions for generic future invalid-kind rejection and generic coroutine readable wait
Tests run: gcc -m64 -c xrt.c -I.; gcc -m64 -c test.c -I.; gcc -fsyntax-only dev/test_xnet2_stage.c -I.; dedicated core-mode compile/run harness using xrt.h + lib/xnet_sync.h + test/test_xnet2_sync.h on Windows
Benchmarks run: none
Blockers: this is only the first generic layer for stream wait surfaces; a broader reusable socket/port wait-source abstraction is still deferred
Next steps: continue collapsing stream/socket wait surfaces toward a reusable wait-source abstraction, now that stream waits have both specialized wrappers and a generic Ex entrypoint
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Extend the generic stream wait abstraction to synchronous thread callers
Tasks touched: xnet-sync future bridge, stream wait-source API shaping, sync/coroutine lifecycle unification
Decisions made: D-053
Code paths changed: updated lib/xnet_sync.h to add xrtNetFutureWaitUntil(), xrtNetStreamWaitEx(), xrtNetStreamWaitTimeoutEx(), and xrtNetStreamWaitUntilEx(); these sync waits reuse the same pending waiter cancellation path as coroutine timed waits; expanded test/test_xnet2_sync.h with guarded regressions for generic sync readable wait and generic sync timeout followed by successful re-registration
Tests run: gcc -m64 -c xrt.c -I.; gcc -m64 -c test.c -I.; gcc -fsyntax-only dev/test_xnet2_stage.c -I.; dedicated core-mode compile/run harness using xrt.h + lib/xnet_sync.h + test/test_xnet2_sync.h on Windows
Benchmarks run: none
Blockers: stream wait abstraction is now shared by future, coroutine, and sync-thread callers, but a broader socket/port-level reusable wait-source model is still deferred
Next steps: continue collapsing stream/socket wait surfaces toward a reusable wait-source abstraction, now that stream wait contracts are unified across future/coroutine/sync-thread layers
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Add the first minimal cross-surface wait-source object above future and stream waits
Tasks touched: xnet-sync wait-source API shaping, sync/coroutine lifecycle unification, xnet sync regression coverage
Decisions made: D-054
Code paths changed: updated lib/xnet_sync.h to add xnetwaitsrc, xrtNetWaitSourceNone()/Future()/Stream(), xrtNetWaitSourceWait()/WaitTimeout()/WaitUntil(), and xrtNetWaitSourceWaitCo()/WaitCoTimeout()/WaitCoUntil(); expanded test/test_xnet2_sync.h with guarded regressions for future-backed sync wait-source wait, future-backed coroutine wait-source wait, and invalid wait-source kind rejection
Tests run: gcc -m64 -c xrt.c -I.; gcc -m64 -c test.c -I.; gcc -fsyntax-only dev/test_xnet2_stage.c -I.; dedicated core-mode compile/run harness using xrt.h + lib/xnet_sync.h + test/test_xnet2_sync.h on Windows
Benchmarks run: none
Blockers: xnetwaitsrc currently unifies only future and stream wait-kind surfaces; broader socket/port-level wait sources remain deferred
Next steps: continue promoting stream/socket readiness contracts into reusable wait-source objects now that future and stream waits share one minimal wrapper surface
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Extend xnetwaitsrc regression coverage to timed future and timed stream waits
Tasks touched: xnet-sync wait-source regression coverage, sync/coroutine timeout lifecycle verification
Decisions made: D-054
Code paths changed: expanded test/test_xnet2_sync.h with guarded regressions for wait-source-backed future coroutine timeout, wait-source-backed readable stream sync timeout, and wait-source-backed readable stream coroutine timeout; each timeout test reuses the same surface after timeout to verify waiter detach completed before return
Tests run: gcc -m64 -c xrt.c -I.; gcc -m64 -c test.c -I.; gcc -fsyntax-only dev/test_xnet2_stage.c -I.; dedicated core-mode compile/run harness using xrt.h + lib/xnet_sync.h + test/test_xnet2_sync.h on Windows
Benchmarks run: none
Blockers: xnetwaitsrc coverage is now broader, but the object still wraps only future and stream wait-kind surfaces; broader socket/port-level wait sources remain deferred
Next steps: continue promoting stream/socket readiness contracts into reusable wait-source objects, with timed wait-source wrappers now covered for both future and readable stream paths
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Extend xnetwaitsrc timed coverage to all remaining stream wait kinds
Tasks touched: xnet-sync wait-source regression coverage, coroutine timeout/deadline waiter lifecycle verification
Decisions made: D-054
Code paths changed: expanded test/test_xnet2_sync.h with guarded regressions for wait-source-backed stream drain timeout, stream close deadline, and stream writable deadline; each test first times out or reaches deadline, then re-registers the same wait surface and verifies the retried wait completes successfully
Tests run: gcc -m64 -c xrt.c -I.; gcc -m64 -c test.c -I.; gcc -fsyntax-only dev/test_xnet2_stage.c -I.; dedicated core-mode compile/run harness using xrt.h + lib/xnet_sync.h + test/test_xnet2_sync.h on Windows
Benchmarks run: none
Blockers: xnetwaitsrc timed coverage now spans readable, writable, drain, and close stream waits, but the object still wraps only future and stream wait-kind surfaces; broader socket/port-level wait sources remain deferred
Next steps: continue promoting stream/socket readiness contracts into reusable wait-source objects, now that all four stream wait kinds have timed wait-source regression coverage
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Align xnetwaitsrc timed regression coverage across both sync-thread and coroutine entrypoints
Tasks touched: xnet-sync wait-source regression coverage, timed future/stream lifecycle verification
Decisions made: D-054
Code paths changed: expanded test/test_xnet2_sync.h with guarded regressions for wait-source-backed future sync timeout/deadline and for wait-source-backed stream drain timeout, stream close deadline, and stream writable deadline on the sync-thread path; all timed tests verify the wrapped wait surface remains reusable after timeout/deadline returns
Tests run: gcc -m64 -c xrt.c -I.; gcc -m64 -c test.c -I.; gcc -fsyntax-only dev/test_xnet2_stage.c -I.; dedicated core-mode compile/run harness using xrt.h + lib/xnet_sync.h + test/test_xnet2_sync.h on Windows
Benchmarks run: none
Blockers: xnetwaitsrc now has broad timed coverage across future plus all four stream wait kinds on both sync-thread and coroutine entrypoints, but the object still wraps only future and stream wait-kind surfaces; broader socket/port-level wait sources remain deferred
Next steps: continue promoting stream/socket readiness contracts into reusable wait-source objects, with the current wait-source layer now broadly regression-covered instead of only structurally unified
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Collapse specialized coroutine stream-wait wrappers onto the generic wait-kind path
Tasks touched: xnet-sync coroutine wait dispatch, stream wait-source implementation consolidation
Decisions made: D-054
Code paths changed: updated lib/xnet_sync.h so xrtNetStreamWaitReadableCo()/WritableCo()/DrainCo()/CloseCo() and their timed variants are now thin wrappers over xrtNetStreamWaitCoEx()/WaitCoTimeoutEx()/WaitCoUntilEx(), making the generic wait-kind path the actual implementation route instead of only a parallel abstraction
Tests run: gcc -m64 -c xrt.c -I.; gcc -m64 -c test.c -I.; gcc -fsyntax-only dev/test_xnet2_stage.c -I.; dedicated core-mode compile/run harness using xrt.h + lib/xnet_sync.h + test/test_xnet2_sync.h on Windows
Benchmarks run: none
Blockers: coroutine stream waits are now implementation-wise consolidated onto the generic path, but xnetwaitsrc still wraps only future and stream wait-kind surfaces; broader socket/port-level wait sources remain deferred
Next steps: continue promoting stream/socket readiness contracts into reusable wait-source objects, now that both tests and implementation dispatch are converging on the same generic wait-kind layer
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Collapse sync/coroutine stream wait dispatch onto the shared xnetwaitsrc core dispatcher
Tasks touched: xnet-sync wait-source implementation consolidation, stream wait dispatch layering
Decisions made: D-054
Code paths changed: updated lib/xnet_sync.h to add internal sync/coroutine wait-source dispatch helpers; xrtNetStreamWaitEx()/WaitTimeoutEx()/WaitUntilEx() and xrtNetStreamWaitCoEx()/WaitCoTimeoutEx()/WaitCoUntilEx() now construct xnetwaitsrc and enter the same core dispatch path used by xrtNetWaitSourceWait*(), removing the old parallel dispatch split between dedicated stream waits and generic wait-source waits
Tests run: gcc -m64 -c xrt.c -I.; gcc -m64 -c test.c -I.; gcc -fsyntax-only dev/test_xnet2_stage.c -I.; dedicated core-mode compile/run harness using xrt.h + lib/xnet_sync.h + test/test_xnet2_sync.h on Windows
Benchmarks run: none
Blockers: dispatch is now unified through xnetwaitsrc for future and stream wait-kind surfaces, but the wait-source object still does not cover broader socket/port-level readiness surfaces
Next steps: continue promoting stream/socket readiness contracts into reusable wait-source objects, now that both regression coverage and implementation dispatch share the same wait-source core
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Add value-returning wait-source APIs on top of the unified future/stream wait dispatcher
Tasks touched: xnet-sync wait-source result model, sync/coroutine wait-source coverage
Decisions made: D-055
Code paths changed: updated lib/xnet_sync.h to add xrtNetWaitSourceWaitValue()/WaitValueTimeout()/WaitValueUntil() plus xrtNetWaitSourceWaitCoValue()/WaitCoValueTimeout()/WaitCoValueUntil(); sync/coroutine stream-wait cores now optionally surface the internal future value before cleanup; expanded test/test_xnet2_sync.h so future and stream wait-source paths validate both status and value propagation, including timed waits leaving value as NULL
Tests run: gcc -m64 -c xrt.c -I.; gcc -m64 -c test.c -I.; gcc -fsyntax-only dev/test_xnet2_stage.c -I.; dedicated core-mode compile/run harness using xrt.h + lib/xnet_sync.h + test/test_xnet2_sync.h on Windows
Benchmarks run: none
Blockers: xnetwaitsrc now carries both status and value for future plus stream wait-kind surfaces, but broader dgram/socket/port-level wait sources are still deferred
Next steps: use the new value-capable wait-source contract as the bridge for the next network object, most likely datagram receive waits before considering raw port-level readiness
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Promote datagram receive into the unified wait-source layer as the first packet-valued network object
Tasks touched: xnet-dgram one-shot packet contract, xnet-sync dgram future/wait-source bridge, sync/coroutine datagram regression coverage
Decisions made: D-056
Code paths changed: updated lib/xnet_dgram.h to add heap-owned xnetdgrampkt, async-hold tracking, and a single recv waiter slot; updated lib/xnet_sync.h to add xrtNetWaitSourceDgramRecv(), xrtNetDgramRecvFuture(), xrtNetDgramRecv()/RecvTimeout()/RecvUntil(), and xrtNetDgramRecvCo*() on top of the existing pending-cancel wait bridge; expanded test/test_xnet2_sync.h with sync recv-helper, wait-source timeout/retry, early-destroy rejection, and coroutine recv-helper regressions
Tests run: gcc -m64 -c xrt.c -I.; gcc -m64 -c test.c -I.; gcc -fsyntax-only dev/test_xnet2_stage.c -I.; dedicated core-mode compile/run harness using xrt.h + lib/xnet_sync.h + test/test_xnet2_sync.h on Windows
Benchmarks run: none
Blockers: datagram wait-source is still single-waiter and mutually exclusive with OnRecv callbacks; backlog buffering and multi-consumer semantics remain deferred
Next steps: keep promoting xnetwaitsrc downward into broader readiness surfaces only after the datagram one-shot packet contract proves stable
```

Progress entry:

```text
Date: 2026-03-13
Author: Codex
Scope: Harden datagram wait-source misuse contracts and explicitly defer listener accept as a wait-source
Tasks touched: xnet-sync datagram regression coverage, wait-source roadmap hygiene
Decisions made: D-057
Code paths changed: expanded test/test_xnet2_sync.h with datagram OnRecv-conflict rejection, second-waiter rejection, sync RecvUntil deadline, and coroutine RecvCoUntil deadline regressions; no listener code was added because accept remains tied to polling-style helpers instead of a real port/worker-owned async contract
Tests run: gcc -m64 -c xrt.c -I.; gcc -m64 -c test.c -I.; gcc -fsyntax-only dev/test_xnet2_stage.c -I.; dedicated core-mode compile/run harness using xrt.h + lib/xnet_sync.h + test/test_xnet2_sync.h on Windows
Benchmarks run: none
Blockers: datagram wait-source now has stronger misuse coverage, but listener accept still lacks async-hold ownership, waiter lifecycle, and port-driven registration/cancellation; accept wait-source work remains intentionally deferred
Next steps: keep hardening completed wait-source contracts and only introduce listener accept waits after listener runtime ownership matches the existing stream/dgram model
```


## 21. Definition of Done

`xnet-v2` is considered ready for upper-layer migration only when:

- `M0` through `M16` are done
- stream and TLS tests pass on both primary desktop targets
- benchmarks are recorded and compared against `xnet-v1`
- no protocol-facing code depends on raw socket primitives
- this document has been updated to reflect delivered behavior
