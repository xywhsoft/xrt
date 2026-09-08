---
num: 95
slug: ws-stream
title: WebSocket Stream: Takeover and the Event Flow
volume: 卷九 Web 协议核心
type: practice
lead: Attach takes over the post-upgrade transport, MessageBegin/Data/End events, reference sending and unified backpressure — the connection object above the frame layer.
api: websocket, websocket_stream, net
---

## Orientation

Chapter 94's frames and messages are **state machines** (transport-free library layers); this chapter's Stream (`xwsstream`) wires them onto a real transport: `xrtWsStreamAttach`/`AttachTls` take over caller references of **already-upgraded** TCP/TLS streams (including the precise splice of "frame data still left in the buffer"), events publish as `MessageBegin → MessageData×N → MessageEnd` — body views are valid only inside the current callback; the Stream **does not aggregate whole messages and does not pre-allocate fixed per-connection buffers** — TCP/TLS's existing block chains feed the frame state machine directly. The send side offers copy and **ref reference** paths (a reference is released only after the socket write truly completes — in multicast, the same reference hangs on N connections with counted release); `SendLimit` is the **unified hard boundary** of WebSocket plus underlying-transport pending, and `Backpressure/Writable/Drain` give the recovery edges. Ping auto-replied, Close guaranteed single-send — the connection object's complete lifecycle.

## Introduction

Chapter 94's closing exercise had you hand-build the frame loop — you find 80% of the code handles "transport wiring": the post-upgrade buffer may already carry frame data, TLS frame headers may straddle records, write backpressure must watch two layers at once, Ping wants an automatic reply, Close may be sent only once... These pipelines, rewritten by every implementation, the Stream layer collects into one `Attach` call plus four event callbacks.

The first thing to understand is the **takeover splice**: the upgrade answer has just been sent, and the receive buffer may already hold the client's eagerly-sent first frame (browsers do this often). The `iPrefix` parameter tells the Stream "the length of HTTP head already validated but not yet consumed" — at takeover it copies the negotiated subprotocol, consumes the head precisely, and **the frame remainder in the same buffer feeds the frame state machine directly** — not one byte wasted, not one misaligned. Failure never partially takes over (the transport reference is returned intact).

## Concepts

### Takeover: Attach's precise splice

```diagram flow
- Precondition: upgrade complete (Chapter 93), holding the caller reference of a TCP/TLS stream
- Attach(transport, config, events, iPrefix = validated head length): copy subprotocol -> consume the head precisely -> the frame remainder feeds the state machine directly
- Events: MessageBegin (type/meta) -> MessageData×N (borrowed views) -> MessageEnd
- Send: Text/Binary/Send (copy) or TextRef/SendRef (reference); hitting SendLimit returns AGAIN
- Final state: Close (single send + timeout + peer snapshot) or Abort
```

TLS's particular handling: a frame header may straddle a TLS record boundary — the Stream accumulates at most 14 bytes of complete frame header (the maximum header size) via the `ReadMore` path bounded by `PlainLimit` — "never stalling just to preserve an incomplete prefix" — Chapter 86's TLS stream and this chapter's frame state machine have their splice details already tuned.

### The event model: views used and dropped

A `MessageData` body view **is valid only inside the current synchronous callback** — the Stream aggregates no whole message (big messages cost no memory) and copies nothing (block chains pass straight through). Your consumption logic must "complete inside the callback" (parse, forward, flush); to retain, copy it yourself. This is the same borrowing discipline as Chapter 86's TLS-stream plaintext consumption and Chapter 90's Body Data views — **in the world of borrowed views, retention is an explicit act**.

### Send's three shapes and unified backpressure

- **copy** (`Text/Binary/Send`): the payload is copied into the queue — the simple path for small messages.
- **ref** (`TextRef/BinaryTake/SendRef` etc.): pass an `xnetref`{data, length, release callback, context} — **zero copying while queued; the release callback fires only after the socket write truly completes**. In multicast, the same reference hangs on N connections (counted release) — one allocation, N sends.
- **compressed send** (`TextCompressed` etc.): a Deflater created on demand — the path after permessage-deflate negotiation (Chapter 96 covers the negotiation).

Backpressure is a **unified watermark**: `SendLimit` counts WebSocket pending and underlying-transport pending together — reaching the cap returns `XNET_RESULT_AGAIN`; the `Backpressure`/`Writable`/`Drain` events give the recovery edges (entering backpressure, writable again, drained). In the same lineage as Chapter 67's TCP write budget and Chapter 86's TLS SendLimit — one watermark per layer, identical semantics.

### Control frames and the close protocol

Ping is **auto-replied** with Pong (the application is unaware); `Ping` may be actively sent. Close's guarantees: **single send** (a repeated Close never reaches the wire), a timeout mechanism (a peer not answering reaches the final state per timeout), and a peer final-state snapshot (CloseInfo: code + reason, for the application to read). The close codes' semantic domains and write-side validation belong to Chapter 96's composition chapter — here remember "Close called once, the state machine handles the protocol".

### Lifecycle and introspection

`Ref/Destroy` reference counting (consistent with the other connection objects); the introspection family: `State/Role/Protocol/Worker/Tcp/TcpRef/Tls/TlsRef/Deflate/Pending/Writable` — every public fact of the connection; `Pause/Paused/Resume` flow control (pause receiving, pairing with downstream rate limits).

## Examples

### First complete program: reference sending — released only after the write

The program below is from `examples/websocket/stream_ref/main.c` — the standard sample of the zero-copy send path:

```embed path="examples/websocket/stream_ref/main.c" title="examples/websocket/stream_ref/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/websocket/stream_ref/main.c -lws2_32 -liphlpapi
WebSocket Connection reference example is ready
```

**What just happened.** (1) The business payload is allocated by `xrtMalloc` (mocking a message-shaped workload from upstream); `xnetref` packages {pointer, length, the `releasePayload` callback, context}. (2) `xrtWsStreamBinaryRef(连接, 负载, Ref)` (connection, payload, Ref) enqueues by reference — **the payload is not copied while it sits in the Worker queue**; only after the send truly completes (written into the socket) is `releasePayload` called and `xrtFree` returns it. (3) Why this matters: in high-throughput scenes the copy path costs an allocation + copy per send; the ref path allocates once, zero transit copies.**The multicast upgrade** (the head comment points the way): the same reference hung on N connections — internal counted release; only the last write's completion truly frees. (4) `Pending`/`Writable` pair up to await draining — how to read the unified backpressure.

The companion `examples/websocket/stream_tour` is the Stream layer's full-feature tour (loopback, both ends): configuration validation, dual-end Attach, 64-byte echo, automatic and manual Pong, pause-resume, the three compression shapes, clean close (code=1000) — run it as this chapter's **executable contract table**.

### Second complete program: an extension-negotiation preview

The second program is from `examples/websocket/extension/main.c` — iterating post-upgrade extension parameters, paving Chapter 96:

```embed path="examples/websocket/extension/main.c" title="examples/websocket/extension/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/websocket/extension/main.c -lws2_32 -liphlpapi
extension=permessage-deflate
  parameter=client_max_window_bits
extension=x-trace
```

**What just happened.** (1) `Sec-WebSocket-Extensions`' two-level structure: the outer level splits extensions by `,`, the inner splits parameters by `;` — isomorphic with Chapter 91's field-family syntax, but the extension level has its own iterator (custom extensions like `x-trace` are also expressed as `name` + parameters). (2) `client_max_window_bits` is one of permessage-deflate's negotiation parameters — Chapter 96 covers negotiation and the compression chain in full; here first build the awareness that "extensions are properties of the post-upgrade connection, and negotiation decides Stream behavior (compression paths, RSV bits)". (3) Extension iteration is pure field-layer work — debugging proxies and compliance checkers can audit negotiation results without building a connection.

## Contracts

- **Takeover atomicity**: a failed Attach never partially takes over (the transport reference returned intact); `iPrefix` precisely consumes the validated head, and the frame remainder in the same buffer feeds the state machine directly.
- **Event views**: a `MessageData` body is valid only inside the current callback; no whole-message aggregation, no per-connection fixed buffer — block chains pass straight into the frame state machine.
- **TLS cross-record**: frame headers crossing TLS records accumulate via `ReadMore` (≤14 bytes, bounded by PlainLimit) — no stalling, no loss.
- **Send's three shapes**: copy (copied into queue) / ref (reference + deferred release callback) / compressed (Deflater on demand); capacity and budget semantics as everywhere in the library.
- **Unified backpressure**: `SendLimit` counts WS plus transport pending together; AGAIN + Backpressure/Writable/Drain edge events.
- **Reference sending**: `xnetref`{data, length, release, context}; released only after the socket write; one reference may hang on N connections with counted release.
- **Ping/Pong**: a received Ping auto-replies Pong; active Pings may be sent; flow control Pause/Paused/Resume.
- **Close guarantees**: single send, timeout mechanism, peer final-state snapshot (CloseInfo); Abort aborts immediately.
- **Introspection family**: State/Role/Protocol/Worker/Tcp(+Ref)/Tls(+Ref)/Deflate/Pending/Writable.
- **Trimming**: `WEBSOCKET_STREAM` independent of the frame layer; `_REF`/`_TLS`/`_DEFLATE` sub-features as needed.

## Pitfalls

### Pitfall 1: storing MessageData's view for asynchronous processing

Symptom: midway through processing, an asynchronous queue crashes or reads garbage — the view expired when the callback returned.

Cause: the Stream neither aggregates nor copies — the view points at the receive block chain's current position, which the next event may overwrite. Asynchronous processing must **copy the data inside the callback** and enqueue the copy.

```c bad
onMessageData(pStream, Data, ...) {
	queue_push(g_Queue, Data);   /* storing the view: dangling on return */
}
```

```c good
onMessageData(pStream, Data, ...) {
	bytes pCopy = (bytes)xrtMalloc(Data.Size);
	memcpy(pCopy, Data.Data, Data.Size);
	queue_push(g_Queue, pCopy);   /* the copy may go async */
}
/* better: finish consuming synchronously (parse/forward), introduce no queue */
```

### Pitfall 2: releasing the payload early after a ref send

Symptom: the peer occasionally receives scrambled frames — the payload was freed while still queued, before the socket write.

Cause: the ref path's contract is "the release callback is called by the Stream" — after enqueueing, the caller **no longer owns** the payload. An early free or a double free both break ownership.

```c bad
xrtWsStreamBinaryRef(pStream, PayloadView, &Ref);
xrtFree(pBuffer);   /* freed right after enqueue: the queued reference dangles */
```

```c good
/* ownership transfers with the reference; free inside the releasePayload callback */
xrtWsStreamBinaryRef(pStream, PayloadView, &Ref);
/* from here pBuffer is the Stream's business - touch it no more */
```

### Pitfall 3: reading SendLimit as a WS-layer watermark alone

Symptom: you tally "WS pending" yourself to judge backpressure, and it never matches the actual AGAIN timing — the TLS/TCP layers' pending also consumes the budget.

Cause: `SendLimit` is a **unified watermark** (WS + underlying transport). Per-layer accounting misses the transport-side queueing; the `Pending`/`Writable` events are simply the whole truth.

```c bad
if ( my_ws_queue_size() < MY_LIMIT ) {
	send_more();   /* the transport layer may long be over the limit */
}
```

```c good
if ( xrtWsStreamSend(pStream, ...) == XNET_RESULT_AGAIN ) {
	/* await the Writable/Drain events - the unified watermark's recovery edges */
	return;
}
```

## Exercises

### Basic: run the tour

Run stream_tour and stream_ref; against stream_tour's head comment, check off each verified behavior (echo/Pong/pause/compression/close). Acceptance criteria: you can point out the event or API behind each behavior.

### Advanced: the echo service in both shapes

Implement echo on Stream events: a copy version (after MessageEnd, Text back) and a ref version (MessageData's view copied into a reference and sent back). Load-test and compare the two versions' allocation counts (Chapter 6). Acceptance criteria: semantics identical; the ref version allocates significantly fewer times per message; under backpressure (a slow client) both stay stable.

### Challenge: a broadcaster

Implement `broadcast(连接数组, 消息)` (connection array, message): one message reference-sent to N connections — a single `xrtMalloc` payload, N `xnetref` counted releases; handle partial-connection backpressure (AGAIN connections skipped or cached for retry — pick a policy and comment why). Acceptance criteria: 8-connection broadcast with zero extra copies (statistics verified); backpressured connections don't disturb the others' sends; after all writes the payload is released exactly once.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Takeover | Attach/AttachTls take over the upgraded transport; iPrefix splices the buffer remainder precisely; failure never partially takes over |
| Event flow | MessageBegin → MessageData×N (views valid only inside the callback) → MessageEnd; no aggregation, zero pre-allocation |
| TLS splice | frame headers crossing records accumulate via ReadMore (≤14B, PlainLimit-bounded) |
| Send's three shapes | copy (copied) / ref (reference with deferred release) / compressed (Deflater on demand) |
| Unified backpressure | SendLimit counts WS + transport together; AGAIN + Backpressure/Writable/Drain |
| Reference sending | xnetref{data, length, release, context}; released only after the write; N-connection counted release possible |
| Control frames | Ping auto-replies Pong; Close single send + timeout + peer snapshot |
| Flow control & introspection | Pause/Resume; the State/Role/Protocol/Pending/Writable family |
| Extension properties | negotiation results iterable (Chapter 96's permessage-deflate) |
| Trimming | STREAM independent of the frame layer; REF/TLS/DEFLATE sub-features |
