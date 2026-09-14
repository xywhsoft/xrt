---
num: 110
slug: xws-conn
title: xws (Part 1): Connection Management and the Full-Chain Tour
volume: 卷十一 其他扩展库
type: practice
lead: xwsconn takes over handshaked connections, a seven-line full-chain tour, the Future/coroutine bridge, pause/resume flow control, and a protocol-driven close — the connection layer of the WebSocket extension library.
api: xws-websocket_runtime, xws-websocket_http, net
---

## Orientation

Volume 9's Chapters 95–97 covered WebSocket's **protocol core** (frames/messages/streams); the xws extension library completes the **engineering connection layer** on top: `xwsconn` takes over a TCP or TLS stream whose handshake is already done — it is not responsible for URL, DNS, HTTP requests, certificate policy, or redirects (those belong to Chapter 94's upgrade and Volume 8's TLS); it manages only the connection's message events, backpressure, close, and introspection. Three blocks in this chapter: **events and view discipline** (message start/data/end, Ping/Pong/Close events; data views valid only inside callbacks); **flow control and the send contract** (pause/resume receive flow control, high/low watermarks and the AGAIN result — inheriting the network stream's full vocabulary); **the full-chain tour** (`connection_tour`'s seven self-check lines covering offline building, a live connection pair, nine send kinds, the async family, auto-Pong, pause/resume, and a clean close — this chapter's executable contract table). The Future/coroutine bridge and the close protocol close the chapter.

## Introduction

xws's division of labor is worth drawing first: the **core library** (Volume 9) provides frames, messages, the handshake, extension negotiation, permessage-deflate — composable protocol parts; the **xws extension layer** (from this chapter) wires those parts onto real connections and adds engineering facilities (connection events, reference lifecycle, Future waiting, connection groups, server routing). The contract docs state plainly that "HTTP client/server adapters, routing, and framework-style convenience objects have left the core" — the extension layer is positioned as the **rebuilding zone for high-level capabilities**, and must not let the object model pollute the frame and direct send/receive paths backward. This "thin core, thick extension" layering is fully isomorphic to xhttp (protocol core in Volume 9, application layer in Volume 10).

The connection layer's first design decision is **take-over, not creation**: `xwsconn` receives "a stream whose handshake is already complete" — who completed the handshake (Chapter 94's upgrade flow, xws server routing (Chapter 113)) does not matter; the connection layer faces only "a ready transport". This lets it stand equally above TCP and TLS (Chapter 97's TLS take-over path).

## Concepts

### Event model and view discipline

Connection events: message start, data fragment, message end, Ping, Pong, Close, error, final close — more than Chapter 96's Stream layer, Ping/Pong get **independent events** (control frames are not only auto-answered; they are also visible). **View discipline** matches the whole library: data views in a callback are valid only during the callback; saving across callbacks requires copying or taking over an owning buffer. The role rules inherit from the core layer: the server requires client frames masked, the client requires server frames unmasked — the role is fixed when the connection is established, never guessed per frame.

### Flow control: pause/resume and watermarks

```diagram flow
- Receive: the message event stream -> application consumption
- Slow consumption: pause suspends receiving -> the peer feels TCP backpressure -> memory stays flat
- Resume: resume continues - never solve a slow consumer with an infinitely growing application queue
- Send: inherits the network stream's high/low watermarks, max queued bytes, and the AGAIN result contract
```

This is the connection-layer implementation of the "stop consuming = stop receiving" discipline (Chapter 62's skeleton economics). The send path's AGAIN and watermark vocabulary is identical to Chapter 96 — having learned the Stream layer, there are zero new words here.

### The three send-ownership states (a preview of the send path)

copy (copied before return — short control frames/short messages), ref (release called exactly once when the queue drains it — static or shared large blocks), take/buffer (handing over already-allocated memory or a network buffer chain) — Chapter 111 expands the writer forms; this chapter first establishes the atomic rule "before acceptance ownership stays with the caller; after acceptance only the connection releases".

### Future and coroutine bridge

Futures wrap only sends, closes, drains, and final-state waits **on an already-established connection** — no hidden HTTP client is created. Two disciplines: **cancellation of a wait ≠ destroying the connection** — the caller decides per the specific operation's contract whether to keep using it, close normally, or abort; coroutines wait through the generic Future/Wait system (Chapter 58) — **no second scheduler is maintained inside WebSocket**. The `connection_future` example family demonstrates the waiting forms.

### Close protocol and error domains

Normal close order: send or answer Close → stop sending new data → wait for the peer's Close or the deadline → shut the write direction → wait for the transport to end. Protocol violation, limit excess, and underlying failure may abort directly — but **every accepted reference must be released exactly once** (the terminal-state guarantee of reference ownership). Error model: protocol errors use stable WebSocket-domain error codes and map to RFC Close codes where possible; network/TLS/application-callback errors **keep their own error domains** — never flattened into one boolean cause (Chapter 4's cause chain honored at the protocol layer).

## Examples

### First complete program: the seven-line full-chain tour

The program below is from `examples/websocket/connection_tour` — a full-feature self-check of the connection layer after an xhttp server and client loop back into each other:

```embed path="extlibs/xws/examples/websocket/connection_tour/main.c" title="extlibs/xws/examples/websocket/connection_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xws/single -include xws.h impl.c extlibs/xws/examples/websocket/connection_tour/main.c -lws2_32 -liphlpapi
conn-tour: offline request builders ok
conn-tour: live pair upgraded, introspection ok
conn-tour: sync send x9 + writer-take delivered ok
conn-tour: async family + wait barrier ok
conn-tour: pong observed via auto-pong ok
conn-tour: pause main/callback held, resume delivered ok
conn-tour: close handshake clean on both peers ok
```

**What just happened.** Seven lines, seven contract classes. (1) **Offline building**: request/response materials can be constructed without a connection (the handshake layer's pure-function face — Chapter 94). (2) **Live connection pair**: an xhttp server and client complete the upgrade over loopback; introspection (role/protocol/state) runs **inside the owning Worker's callback** — the Worker-affinity discipline of synchronous calls. (3) **Nine synchronous sends + writer-take**: copy/ref/take × text/binary plus the writer path (Chapter 111) all deliver — the Take payload's release callback fires **exactly once** (terminal-state ownership verification). (4) **Async family + wait barrier**: Future sends and waits (cancelling does not destroy the connection). (5) **Auto-Pong**: a Ping observes the automatic answer — control-frame automation. (6) **Pause/resume**: the main callback pauses receiving, data is held, delivery resumes after — flow-control semantics. (7) **Clean close**: both peers complete the Close handshake. One line, one contract; seven lines, all green — this chapter's contract table is exactly that output.

### Second complete program: the release contract of reference sending

The second program is from `examples/websocket/connection_ref` — the ownership template of the ref path:

```embed path="extlibs/xws/examples/websocket/connection_ref/main.c" title="extlibs/xws/examples/websocket/connection_ref/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xws/single -include xws.h impl.c extlibs/xws/examples/websocket/connection_ref/main.c -lws2_32 -liphlpapi
WebSocket Connection reference example is ready
```

**What just happened.** (1) The `xnetref` quadruple {pointer, length, `releasePayload` callback, context} — the payload is independently allocated by `xrtMalloc` (not borrowing the caller's buffer). (2) `xrtWsConnBinaryRef(连接, &Ref)` (connection) queues the reference — **zero copies while queued, release exactly once after the socket write**; on send failure (not accepted) ownership returns — the sample's failure branch calls `xrtFree` itself (contract: unaccepted stays with the caller). (3) This sample is a function template "called from a connected Worker's callback" (main only verifies readiness) — the real call sites live in message/routing callbacks. Chapter 96 covered the core library's ref sending; this is the same semantics at the connection layer — the vocabulary is fully shared.

## Contracts

- **Take-over boundary**: xwsconn takes over handshaked TCP/TLS streams; it does not manage URL/DNS/HTTP/certificates/redirects — the layers above and below each stay in place.
- **Event model**: message start/data/end + Ping/Pong/Close/error/final state; data views valid only inside callbacks; role fixed at establishment.
- **Flow control**: pause/resume; unbounded application queues forbidden; sending inherits the watermark and AGAIN contracts.
- **Ownership atomicity**: copy/ref/take three states; unaccepted stays with the caller, after acceptance the connection releases exactly once.
- **Future bridge**: wraps only operations on established connections; cancelling a wait ≠ destroying the connection; coroutines go through the generic Future/Wait — no second scheduler.
- **Close order**: Close → stop new data → wait for peer/deadline → shut write direction → wait for transport; abort also guarantees reference release.
- **Error domains**: protocol errors map to WS-domain codes + RFC Close codes; network/TLS/application errors keep their own domains — never flattened.
- **Trimming**: connection/ownership/async/TLS/grouping in independent tables (the frame layer pulls in no networking — a continuation of the core trimming table).

## Pitfalls

### Pitfall 1: pause without resume (or the peer times out)

Symptom: the client "mysteriously stops receiving messages" — the server paused, the consuming logic stalled, and the connection half-lives until timeout.

Cause: pause means "not receiving for now" — it must be paired with an explicit resume path (async consumer finished, downstream ready). While suspended, the peer's view is a stuck connection.

```c bad
onMessage(...) {
	if ( busy() ) {
		xrtWsConnPause(pConn);   /* paused, nobody resumes */
	}
}
```

```c good
onMessage(...) {
	if ( busy() ) {
		xrtWsConnPause(pConn);
		defer_resume_on_ready(pConn);   /* an explicit resume trigger point */
	}
}
```

### Pitfall 2: treating a Future cancel as a connection close

Symptom: after a send Future wait times out, the connection is Destroyed outright — the half message the peer is receiving gets cut in two, and a double release is possible.

Cause: cancelling a wait only means "stop waiting" — the connection and the ownership of accepted data are intact. Decide per the operation's contract: keep using it (wait on the next Future), close normally, or abort.

```c bad
if ( wait_timeout(pSendFuture) ) {
	xrtWsConnDestroy(pConn);   /* cancel != destroy: possible double release / half message */
}
```

```c good
if ( wait_timeout(pSendFuture) ) {
	/* was the data accepted? the connection state tells you - then choose the close style */
	if ( !xrtWsConnClose(pConn, Code, Reason) ) {
		xrtWsConnAbort(pConn);   /* abort only when close fails */
	}
}
```

### Pitfall 3: calling synchronous sends from a non-Worker thread

Symptom: occasional state scrambling — synchronous sends are designed to run on the connection's owning Worker (the tour's line-2 comment says so explicitly).

Cause: the connection object is not multithread-shared mutable state — cross-thread operations go through the Future/asynchronous family (they dispatch correctly inside). The same source as Chapter 86's session layer and Chapter 104's server-event Worker-serial discipline.

```c bad
/* a synchronous send called directly from an arbitrary thread */
xrtWsConnText(pConn, Msg);   /* not the owning Worker: a race */
```

```c good
/* cross-thread: go through the Future/async family (dispatched to the connection's Worker internally) */
/* see the websocket_http_future module: the waiting semantics of the ConnectAsync family */
```

## Exercises

### Basic: annotate the tour line by line

Run connection_tour and write one sentence per output line stating "which contract it verified". Acceptance criteria: all seven contracts can be pointed at the corresponding entry in this chapter's Contracts section.

### Advanced: a pause-resume stress pair

On the tour's loopback skeleton: the consumer randomly pauses 0–50 ms then resumes, the sender drives a 1000-message workload at full speed — tally deliveries and losses. Acceptance criteria: zero loss (pause only delays); memory constant (Chapter 6 stats); from the peer's view, only throughput fluctuates.

### Challenge: a dual-channel proxy

Messages received on connection A are forwarded to connection B (and vice versa) — with pause linkage across the two connections: when B is busy, pause A. Acceptance criteria: zero forwarding loss; when B slows, A's receiving pauses in step (memory constant); when either side closes, the other finishes cleanly (Close propagates).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Division of labor | core library (protocol parts, Chapters 95–97) + xws (the connection/group/routing engineering layer) |
| Take-over form | a handshaked TCP/TLS stream; no URL/DNS/HTTP/certificates |
| Event model | three message phases + Ping/Pong/Close/error/final; views valid only inside callbacks |
| Flow control | pause/resume; unbounded queues forbidden; AGAIN result + watermarks as everywhere |
| Three ownership states | copy/ref/take; unaccepted stays with the caller; after acceptance released exactly once |
| Future bridge | wraps only established-connection operations; cancel ≠ destroy; coroutines go through the generic system |
| Close order | Close → stop sending → wait peer → shut write → wait transport; abort still guarantees reference release |
| Error domains | WS-domain codes + RFC Close codes; network/TLS/application each keep their domain |
| The tour | connection_tour's seven lines = an executable table of seven contract classes |
| Trimming | connection/ownership/async/TLS/grouping in independent tables |
