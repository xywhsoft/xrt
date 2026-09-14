---
num: 87
slug: tls-stream
title: TLS Streams: The Composition Layer over TCP
volume: 卷八 安全
type: practice
lead: StreamAccept takes over a TCP stream and completes the server handshake in one step — event-driven plaintext read/write and hard backpressure, the composition template of session machine and transport.
api: tls, tls_stream, net
---

## Orientation

Chapter 86's session machine touches no network; Chapter 82's client dial already quietly used the Stream layer. This chapter finishes the Stream layer (`xtlsstream`) head-on: the **server shape** — `xrtTlsStreamAccept` in the TCP Accept callback builds the "TCP stream + TLS session machine" composite in one step, after which the business sees only plaintext read/write; the **event model** — the four callbacks Open/Read/Writable/Close (the same set as Chapter 82's client); **backpressure** — plaintext receive Available + short-write waiting on Writable; a slow client never balloons server memory; the **lifecycle** — shared configuration reference-counted, released together after the listener and every connection close. This is another redemption of the library-wide "skeleton composition" pattern: Engine (Chapter 64) + Listener/Stream (Chapter 67) + TLS session machine (Chapter 86) assembled into a complete service `openssl s_client` can connect to directly.

## Introduction

Turning Chapter 86's echo sparring into a real service requires only "transport wiring": Accept a TCP stream → build the composite → the handshake runs automatically → event callbacks deliver plaintext. It sounds like a thin layer, but every spot is a trap: who feeds the handshake bytes? Where does plaintext consumption pause when write backpressure jams? How do connection close and listener close coordinate without leaks? The Stream layer standardizes this wiring — what you write is the business logic inside the `Echo` callback; lifecycle and backpressure are managed by the composite.

The service shape is worth seeing: one process, one Engine, one Listener, N TLS connections **sharing** the same server configuration (identity/ALPN) and event table — reference-counted configuration makes "assemble once, reuse across N connections" zero-copy. This lines up with Chapter 83's identity sharing and Chapter 85's verifier sharing: **all of TLS's heavy objects are immutable shared values**.

## Concepts

### Building the composite: Accept and Dial sides

- **Server**: `xrtTlsStreamAccept(TCP流, 服务端配置, 流配置, 事件表, 用户数据, &流)` — inside the TCP Accept callback, building the composite in one step; the handshake runs automatically, and the Open callback fires on success.
- **Client**: `xrtTlsDial/DialAsync` (Chapter 82) internally is exactly "TCP dial + StreamAttach in the client direction" — the client has no separate Attach example because dialing already inlines it.

Both sides share the `xtlsstream` type and every application interface — Send/Available/Buffer/Consume/Close/Abort/State (Chapter 82's cheat sheet). They differ only in how they are built and their direction.

### The event model and hard backpressure

The four callbacks are fully isomorphic to the TCP stream's (Chapter 67): `Open` (handshake complete/READY), `Read` (plaintext arrived), `Writable` (send budget recovered), `Close` (the sole final state: `xnetresult` + structured error). **The hard-backpressure mechanism**: the plaintext receive area has a watermark (`Available` non-zero means pending); until the business consumes it, the receive side does not advance — the echo service binds "consuming plaintext" and "sending plaintext back" into one loop: `Send` Span by Span, and on a short write (`XTLS_AGAIN`) **pause consumption**, awaiting `Writable` to resume — the slow client's TCP window shrinks → TLS records can't go out → plaintext consumption pauses → the receive watermark holds → memory constant. This is Chapter 62's "pump zero-blocking" promise redeemed on TLS.

### Configuration sharing and lifecycle

```diagram flow
- Assembly: identity (Chapter 83) + server config + stream config + event table — all on the stack or shared objects
- Admissions: StreamAccept once per TCP connection; the composite holds configuration references
- Running: N connections concurrent; callbacks execute on Engine Workers (Chapter 64)
- Closing: each connection's Close decrements the count; the listener's close sets a flag; both reaching zero releases the shared objects together
```

The shared configuration may not be released before the Listener and all connections close — the example orchestrates this rendezvous with atomic counters (Connections/ListenerClosed). The per-connection rule matches Chapter 82: Close/Abort → wait `XTLS_STREAM_CLOSED/FAILED` → Destroy.

### Three TLS-specific wiring details

Three implementation joins worth knowing: **handshake bytes** are fed automatically by the composite (you never called Feed — after StreamAccept the handshake is "the transport's business"); the **transport query** `xrtTlsStreamTransport(流)` lends the underlying TCP stream — querying the peer address (`xrtNetStreamRemote`) for logging in the Open callback is the standard move; **two-tier close** — `StreamClose` goes through close_notify draining (Chapter 82), and the Close callback fires once at the composite's final state. Configuration comes in two parts: `xtlsserverconfig` (protocol face: identity/ALPN) and `xtlsstreamconfig` (stream face: buffer watermarks etc.) — protocol-transport separation of concerns down to the configuration layer.

## Examples

### First complete program: the event-driven TLS echo service

The following program comes from `examples/tls/stream/main.c` — a complete TLS 1.3 Echo service connectable with `openssl s_client`:

```embed path="examples/tls/stream/main.c" title="examples/tls/stream/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/tls/stream/main.c -lws2_32 -liphlpapi
usage: stream <rsa|p256|p384|ed25519> ...
```

**What just happened.** (1) **The Echo main loop is the hard-backpressure template**: `Available → Buffer → Front 取 Span → Send(部分受理 iWritten) → Consume(iWritten) → AGAIN 即返回` — both `Read` and `Writable` enter this single no-duplication path; a slow client can never blow up memory. (2) **Accept wiring**: inside the TCP Accept callback, `StreamAccept` builds the composite in one step — this example immediately `Destroy`s (demonstrating ownership: Accept's success hands you a reference, the event table takes over the rest; a real service lets connections run naturally to close). (3) **The transport query in Open**: `StreamTransport` lends the TCP stream to query `Remote` and print "TLS client open: address" — that one bridge is all there is between the TLS layer and the transport layer. (4) **Lifecycle rendezvous**: the Close callback decrements Connections and ListenerClose sets the flag — the main thread waits for both to reach zero before releasing the shared configuration; this is the close-side orchestration of "N connections sharing one assembly". (5) Read side by side with Chapter 67's TCP echo: the event model, Span consumption, and short-write resumption are isomorphic item by item — the only additions are the handshake and two-tier close.

### Second complete program: the Stream-layer full-feature tour

The second program comes from `examples/tls/stream_tour/main.c` — five lines covering the Stream layer's advanced face:

```embed path="examples/tls/stream_tour/main.c" title="examples/tls/stream_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/tls/stream_tour/main.c -lws2_32 -liphlpapi
stream-tour: two clients upgraded, echo verified
client(starttls): vec=9 bytes read=4 consume=5 session+data=ok
client(attach): async-vec=ok future-written=9 pending=0
bounds: 64 -> 86 ciphertext, error=(none)
http1-tls: request+response parsed on stream ok
```

**What just happened.** (1) Line 1 checks two client-admission paths: **starttls** (negotiated upgrade over a cleartext connection — the official answer to Chapter 87's exercise) and **attach** (hanging a TCP stream on directly) — plus the server's Accept, the complete set of the Stream layer's three establishment forms. (2) Line 2 demonstrates Vec batch sending (9 bytes) and Span consumption (read 4, consume 5 — consumption may run ahead of the read boundary; watermark semantics). (3) Line 3 walks asynchronous Vec writing: after the Future completes, `Pending` returns to zero — what Chapter 68's five send tiers' Vec/async tiers look like on a TLS stream (see `stream_tour`). (4) Line 4 measures the ciphertext boundary: 64 bytes plaintext → 86 bytes ciphertext (16-byte AEAD tag + record-header overhead) — a real anchor for capacity planning. (5) Line 5 runs a complete HTTP/1 request-response parse on the stream — a self-check specimen chaining Chapters 84–87's primitives, and a preview of Volume 9.

## Contracts

- **Establishment**: server `StreamAccept` (one step inside the Accept callback); client attaches inline via `TlsDial/DialAsync`; both sides share `xtlsstream`'s full application interface.
- **Events**: Open/Read/Writable/Close isomorphic to the TCP stream; callbacks execute on Engine Workers; Close is the sole final-state notice (result + structured error).
- **Hard backpressure**: stalled plaintext consumption stalls reception — Span-by-Span echo + short-write awaiting Writable is the standard echo shape; memory decoupled from the slowest client.
- **Transport bridge**: `StreamTransport` lends the underlying TCP stream (read-only queries: address/state); never cache across callbacks.
- **Two-tier close**: `Close` goes through close_notify draining; `Abort` aborts immediately; a single connection waits CLOSED/FAILED before Destroy; the service level waits "all connections + listener" reaching zero before releasing the shared configuration.
- **Configuration separation**: `xtlsserverconfig` (protocol face) and `xtlsstreamconfig` (stream face) each independent; configuration objects share via reference counting.
- **Handshake automation**: the Feed/Drive loop is executed by the composite — the application never sees ciphertext (stream_tour's boundary measurement is diagnostic-tool usage).
- **Trimming**: `XRT_MODULE_TLS_STREAM` independent (depends on NET) — trim it away when you want only the session machine without the transport composition.

## Pitfalls

### Pitfall 1: copying all the plaintext in the Read callback before processing slowly

Symptoms: memory spikes under high throughput — every connection's Read callback copies the whole receive area into its own queue, hollowing out the backpressure mechanism.

Cause: the Stream layer's backpressure is built on "consuming plaintext (Consume)"; copying without consuming equals not consuming — the receive side keeps advancing, and your queue becomes an unbounded buffer.

```c bad
static void onRead(xtlsstream* pStream, const xnetbuf* pBuffer, ptr pData) {
	queue_push(g_MyQueue, pBuffer);   /* copied away but not Consumed: backpressure voided */
}
```

```c good
static void onRead(xtlsstream* pStream, const xnetbuf* pBuffer, ptr pData) {
	/* consume as much as you can process; leave the rest for the next Read/Writable */
	while ( xrtTlsStreamAvailable(pStream) != 0 ) {
		/* ... Front takes a Span, process + Consume ... */
		if ( 下游满了 ) { return; }    
	}
}
```

### Pitfall 2: releasing the shared configuration the moment the listener closes

Symptoms: the last connection's callback accesses a freed identity/config — a crash exactly when the final client disconnects.

Cause: the shared configuration's reference count sits on every connection — Listener closed does not mean connections closed; release must wait for both counts to reach zero.

```c bad
listenerClose(...) {
	xrtTlsIdentityRelease(g_Identity);  /* active connections still using it */
}
```

```c good
/* atomic-counter rendezvous: release together only when Connections==0 && ListenerClosed */
if ( (xrtAtomic32Load(&Connections, XMEMORY_ACQUIRE) == 0) &&
		(xrtAtomic32Load(&ListenerClosed, XMEMORY_ACQUIRE) != 0) ) {
	release_shared_assembly();
}
/* both the Close and ListenerClose callbacks run this check */
```

### Pitfall 3: storing the TCP stream lent by StreamTransport for long-term use

Symptoms: occasional misbehavior — the cached TCP-stream pointer mismatches after the composite's internal state advances.

Cause: `StreamTransport` is a "transport bridge at this moment" diagnostic/query view, not a long-term handle. For transport capabilities (query address, state), take it fresh at the moment of need.

```c bad
g_SavedTcp = xrtTlsStreamTransport(pStream);   /* saved away */
/* several callbacks later */
send_on_tcp(g_SavedTcp, ...);   /* writing the transport directly, bypassing TLS: state mismatch */
```

```c good
/* query fresh when needed (e.g. logging in Open); sending always goes through the TLS layer's Send */
if ( xrtNetStreamRemote(xrtTlsStreamTransport(pStream), &Remote) ) {
	log_client(Remote);
}
xrtTlsStreamSend(pStream, Data, Size, &Written);  /* plaintext through the front door */
```

## Exercises

### Basic: interop with openssl

Generate a self-signed certificate and run the stream service; connect with `openssl s_client -connect localhost:8443 -alpn http/1.1`, type text to verify the echo; watch the server print the client address. Acceptance: the echo is byte-for-byte correct; after disconnecting (Ctrl-C or Q) the server's Close callback fires, the connection count returns to zero, and the process keeps serving the next connection.

### Advanced: a plaintext proxy (observation on the decryption side)

Insert an "audit" layer before the Echo: in the Read callback tally each connection's plaintext bytes and Span count, and output a summary at Close (total bytes/average Span size/connection duration). Acceptance: 4 concurrent connections each independently tallied; data matches the openssl side's sent volume; the tally copies no plaintext (done on the zero-copy path).

### Challenge: a StartTLS upgrade gateway

Implement a "cleartext handshake command first, then upgrade" gateway: after TCP Accept, don't go TLS immediately — read commands line by line (Chapter 72's line framing); on `STARTTLS\r\n`, call StreamAttach to upgrade to TLS (other commands echo in cleartext mode). Hint: the same connection object before and after the upgrade — compare `stream_tour`'s starttls client path. Acceptance: cleartext-stage commands echo normally; after the upgrade, cleartext commands are refused; openssl completes the post-upgrade encrypted session via the `starttls` hint; connection counts and both stages' resources cleaned up correctly.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| The composite | server `StreamAccept` (one step inside the Accept callback) / client attaches inline via `TlsDial` |
| Event model | Open/Read/Writable/Close isomorphic to the TCP stream; executed on Worker threads |
| Hard backpressure | stop consuming = stop receiving; Span echo + short-write awaiting Writable; memory decoupled from the slowest client |
| Application interface | Send (partial acceptance) / Available/Buffer/Front/Consume / Close / Abort / State |
| Transport bridge | `StreamTransport` lends the TCP stream — query fresh, never cache; sending goes through the TLS layer's front door |
| Configuration separation | xtlsserverconfig (protocol face) / xtlsstreamconfig (stream face); shared via reference counting |
| Service-level lifecycle | all connections + listener reaching zero releases the shared assembly together (atomic-counter rendezvous) |
| Per-connection lifecycle | Close/Abort → wait CLOSED/FAILED → Destroy (Chapter 82's rule) |
| Handshake automation | Feed/Drive executed by the composite; the application never sees ciphertext |
| Trimming | `XRT_MODULE_TLS_STREAM` independent of the session machine; stream_tour is the full-feature self-check |
