---
num: 104
slug: xhttp-server
title: The Server (Part 1): Startup, Connections, and Routing
volume: 卷十 扩展库：xhttp
type: practice
lead: Five-segment timeout protection, the Headers/Body/Request event chain, the Reply direct-answer path and Drain teardown — an HTTP/1 service runtime built on aggregated TCP.
api: xhttp-http_server, xhttp-http_server_router, net
---

## Orientation

After six client chapters (97–102), the perspective flips: now you are the server. `xrtHttpServerStart` assembles an Engine, the aggregated TCP Server, Stream, and the I/O-free protocol state machines into a plaintext HTTP/1 service in one step — dependencies hidden behind no function table (honest trimming). This chapter covers three things: **the configuration face** — five-segment timeouts (Header/Body/Request/Idle/Write — each independently protecting one stage, zero disables); **the event chain** — `Open → Headers → Body → Request → Error/Close`, where `Headers` makes the policy decision before reading the body (buffer/stream/discard/reject/respond directly), and `Body` only receives streaming fragments with a false return meaning 500; **three response paths** — `Reply` (the direct-answer path for fixed bytes, zero temporary containers), `ReplyBody` (the body-source version), and `Respond` (the full builder for dynamic fields and trailers). Routing and middleware are Chapter 105; SSE/streaming live in Chapters 107/108. The protocol state machine's request-head entry underneath is exactly Chapter 90's parser network binding (http1_net — the buffer chain feeds the parser directly, no manual stitching).

## Introduction

The biggest paradigm difference between server and client is **control inversion**: the client "initiates and waits"; the server "is event-driven" — when requests arrive, how many come, whether they carry bodies, whether they are maliciously slow — all decided by the peer. The runtime's design unfolds around this inversion: the five-segment timeouts each defend against one "slow attack" (slow headers HeaderTimeout, slow body BodyTimeout, stuck application RequestTimeout, held-unused IdleTimeout, stalled response WriteTimeout); the body policy is decided at the Headers stage — before routing runs you must state "what happens to this request's body"; application backpressure (`PauseRequestBody`) lets you **not read the peer's body** until your asynchronous consumer catches up — the TCP window shrinks naturally, memory does not grow.

The `Reply` direct path deserves its own sentence: fixed JSON/text responses **create no temporary Reply or Header containers**, require no dictionaries or JSON objects — one call enters the frozen kernel. Health checks, short errors, fixed inter-microservice answers — these "every service has them" paths get the zero-overhead shape.

## Concepts

### Startup and configuration

`xrtHttpServerStart(Engine, 配置, 事件表)` (Engine, config, event table) creates and starts immediately;**a failed startup leaves no partially usable Server** — all logical endpoints either all bind successfully or fail as a whole (multi-endpoint configuration in the Network section). Configuration essentials:

- **Five-segment timeouts** (microseconds): `HeaderTimeout` (header-reading stage), `BodyTimeout` (body reading), `RequestTimeout` (application processing time), `IdleTimeout` (keep-alive idleness), `WriteTimeout` (response without progress) — zero disables the corresponding protection. Every segment of a slow attack has its gate.
- **Connections and caps**: `MaxConnections` (zero = no application-layer cap), `MaxInformations` (queued informational responses per request), `WriteSize` (a zero-copy send lease at a time — **no fixed send buffer reserved per connection**); receive memory is managed by TCP's on-demand `xnetbuf` and hard limits.
- **Network**: fully exposes the TCP Server's multi-endpoints, shared dynamic ports, accept queue, reuse-port — `xrtHttpServerLocal` fetches the real endpoint (the standard posture for a zero listen port, same as Chapter 67); `xrtHttpServerNetwork` lends the underlying `xnetserver` reference (for special scenarios wanting low-level statistics; `xrtNetServerDestroy` when done).

### The event chain and body policy

```diagram flow
- Open: address, Worker, underlying TCP connection facts
- Headers: the decision point before reading the body - buffer / stream / discard / reject / respond directly
  (bodyless requests continue; bodied requests wait at the head boundary for policy)
- Body: fires in streaming mode only - borrowed body fragments; returning false = callback error + 500
  (if a response was already committed, the response wins; PauseRequestBody pauses delivery)
- Request: the complete request arrived - respond synchronously or retain the Connection for later delivery
- Error -> Close: at most one stable error per connection, exactly one final state; Shutdown = all reclamation complete
```

Three key semantics.① **Headers' policy authority**: per-route `xrtHttpConnSetRequestBodyLimit` (the current Exchange's only hard cap) — upload caps are a routing-level decision, not a global constant.② **Body backpressure**: `PauseRequestBody` may be called only inside a Body callback — it expires the moment the current fragment's callback returns (copy to retain); after the asynchronous consumer finishes, `ResumeRequestBody` from **any thread** (a command embedded in the Connection resumes it, allocating no task node); during the pause BodyTimeout keeps counting — a lost consumer cannot hold the connection forever.③ **Response uniqueness**: exactly one final commit per request (Respond/Reply/ReplyBody, choose one); `xrtHttpConnInform` may send 1xx informational responses first (except 101).

### Three response paths and the frozen kernel

- `xrtHttpConnReply(连接, 状态, 类型, 字节)` (connection, status, type, bytes) — the direct answer for fixed responses: copied into a compact Body straight into the HTTP/1 frozen kernel, zero temporary containers. The standard shape for health checks.
- `xrtHttpConnReplyBody(连接, ..., 可选 xhttpbody 引用)` (connection, ..., optional xhttpbody reference) — the body-source version of the same direct path: Borrow/Take/Reference/files/producers avoid copies; unknown lengths go chunked (1.1) or close-framing (1.0). After the call returns, the caller may destroy its own body reference.
- `xrtHttpConnRespond` — the full builder path for dynamic fields and trailers (the mirror of Chapter 99's request builder).

### Lifecycle: Drain and Shutdown

`xrtHttpServerDrain` drains gracefully: no new connections, in-flight requests complete, connections close in turn; the `Shutdown` event is published after **all** protocol objects, transport ownership, and runtime exit, and after accepted Upgrade handover callbacks return — "nothing is still running" is event-guaranteed, not guessed. The teardown template of spinning on `XHTTP_SERVER_CLOSED` matches Chapter 67's TCP.

## Examples

### First complete program: a health-check service

The program below is from `examples/http/server` — the minimal complete service:

```embed path="extlibs/xhttp/examples/http/server/main.c" title="extlibs/xhttp/examples/http/server/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/server/main.c -lws2_32 -liphlpapi
listening on http://127.0.0.1:52173/health
press Enter to drain
```

**What just happened.** (1) The assembly trio: Engine start, `HttpServerConfigInit` (five-segment timeouts at defaults — the conservative default with all protections on), `HttpServerEventsInit` plus two callbacks (Request/Error). (2) Listen address `NetAddrLoopback` + **port 0** (system-assigned) — `xrtHttpServerLocal` fetches the real endpoint to print the URL — Chapter 67's Listener posture reappearing in xhttp. (3) **The Request callback is the entire business**: Target equal to `/health` gets `HttpConnReply` answering JSON directly (one call, zero containers); otherwise a 404 direct answer — both paths in direct-answer form. (4) Enter triggers `Drain`: graceful drain → spin on `XHTTP_SERVER_CLOSED` → destroy — **a service's complete lifetime walked through in one main**, the teaching sample's posture.

### Second complete program: router assembly

The second program is from `examples/http/server_router` — routing plus direct answers:

```embed path="extlibs/xhttp/examples/http/server_router/main.c" title="extlibs/xhttp/examples/http/server_router/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/server_router/main.c -lws2_32 -liphlpapi
（输出路由装配计数并正常退出）
```

**What just happened.** (1) `xrtHttpServerRouterCreate` builds the router; `xrtHttpServerGet(路由器, 路径, 处理器, 数据)` (router, path, handler, data) registers a method+path handler — PUT/POST/wildcards in the same family; `RouterFreeze` freezes into a read-only shared table (the same idea as Chapter 99's "freeze on submit": hot-path dispatch never touch mutable structures). (2) The frozen route table is dispatched by the Server's event chain: request arrives → match → the handler receives the `xhttprouteparam` parameter array (path parameters like the id of `/items/{id}`); no match goes the automatic 404 path. (3) Route-level callbacks share this chapter's Request callback shape — `xhttpserverrequest` snapshot + `HttpConnReply/Respond` response; middleware (`xrtHttpServerUse`) stacks outside routing — Chapter 105 unfolds it.

## Contracts

- **Startup atomicity**: all logical endpoints bind together; failure leaves no partial Server; addresses validated before range opening; caps statically validated without allocation.
- **Five-segment timeouts**: Header/Body/Request/Idle/Write each guard one stage, microsecond units, zero disables; every slow-attack segment has a gate.
- **Event serialization**: all application events execute serially on the connection's Worker; Error at most once (stable error + cause chain), Close exactly once; transport failures follow the fixed Error→Close order.
- **Headers policy**: buffer/stream/discard/reject/respond-directly, five choices; body caps are routing-level (SetRequestBodyLimit sets the current Exchange's only hard cap).
- **Body backpressure**: Pause only inside the Body callback; expires on fragment-callback return; Resume from any thread (embedded command, zero allocation); BodyTimeout keeps counting during the pause.
- **Response uniqueness**: one final commit; Inform may send 1xx (except 101); informational and final responses share the backpressure line.
- **Direct-answer path**: Reply/ReplyBody enter the frozen kernel with zero temporary containers; the caller may destroy the body reference after submission; unknown lengths chunked/close-framed.
- **Lifecycle**: Drain drains gracefully; Shutdown published after all reclamation (Upgrade handover included); the Closed final state awaited by spinning.
- **Trimming**: `XHTTP_FEATURE_HTTP_SERVER` depends on Exchange/Response/TCP Server; streaming outbound, files, TLS, Upgrade are independent composition layers — no second state machine in the core.

## Pitfalls

### Pitfall 1: storing the fragment view inside the Body callback for async processing

Symptom: the asynchronous consumer reads scrambled fragment data or crashes — the same dangling-view family as Chapter 96.

Cause: Body fragments are **borrowed** — they expire when the callback returns. Asynchronous consumption either copies or uses Pause/Resume to let the runtime wait for you.

```c bad
static bool onBody(..., xbytesview Chunk, ...) {
	queue_push(g_Queue, Chunk);      /* storing the view: dangling on return */
}
```

```c good
static bool onBody(xhttpconn* pConn, ..., xbytesview Chunk, ...) {
	if ( !consumer_ready() ) {
		copy_and_queue(Chunk);                 /* copy to retain */
		xrtHttpConnPauseRequestBody(pConn);    /* pause further delivery */
		return true;
	}
	consume_now(Chunk);                          /* synchronous consumption, zero copy */
	return true;
}
/* after the async consumer finishes, any thread: xrtHttpConnResumeRequestBody(pConn) */
```

### Pitfall 2: no WriteTimeout for slow clients

Symptom: a client that disconnects mid-download (without sending RST) holds the connection and buffers for hours — the connection count is drained by such zombies.

Cause: the WriteTimeout default may not cover your deployment shape (zero = protection off). The response stage's "no progress" must have a deadline.

```c bad
xrtHttpServerConfigInit(&Config);
/* WriteTimeout at default/zero - zombie connections hold quota forever */
```

```c good
xrtHttpServerConfigInit(&Config);
Config.WriteTimeout = UINT64_C(30000000);   /* abandon after 30s without response progress */
Config.IdleTimeout = UINT64_C(75000000);    /* keep-alive 75s */
/* all five explicit - protection is a deployment contract, not luck */
```

### Pitfall 3: long blocking operations inside the Request callback

Symptom: a Worker blocked — every other connection on the same Worker stalls (the server-side cost of Chapter 64's Worker economics).

Cause: events execute serially on the Worker; blocking inside a callback (synchronous file IO, external calls) pawns the entire Worker. RequestTimeout only limits the loss; it is not the solution.

```c bad
static void onRequest(..., const xhttpserverrequest* pReq, ...) {
	char* p = blocking_external_call(...);   /* blocks 2s: the whole Worker stops */
	reply(...);
}
```

```c good
static void onRequest(xhttpserver* pS, xhttpconn* pC,
		const xhttpserverrequest* pReq, ptr pD) {
	submit_to_pool(pC, pReq);   /* hand to the execution pool (Chapter 59), retaining the Connection */
	/* after the pool finishes, deliver back to the owning Worker to Respond - RequestTimeout as the floor */
}
```

## Exercises

### Basic: the five-segment timeout experiment

From the server sample, verify item by item: slow headers (1 byte per second), slow body, a client that never reads — observe each corresponding timeout firing and the connection closing. Acceptance criteria: the three rejection times match configuration; the error callback receives the structured error with stage information.

### Advanced: routing and parameters

With the Router, implement `/items/{id}` and `/items/{id}/parts/{part}` two-level path parameters, the Request printing the parameter values; add a PUT handler. Acceptance criteria: path parameters extracted in order; no match 404; method mismatch 405.

### Challenge: an upload-cap gateway

The Headers callback sets body caps per route (`/avatar` 2 MB, `/video` 200 MB, others 1 MB); Body streams and counts bytes; over-cap requests are rejected at the body stage (413) with a clean connection close. Acceptance criteria: the three caps each take effect; over-cap connections occupy no memory (the cap intercepts before consumption); normal uploads received in full.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Startup | Start builds the plaintext service in one step; failure leaves no partial Server; Local fetches the dynamic-port endpoint |
| Five timeouts | Header/Body/Request/Idle/Write each guard one segment; microseconds; zero disables |
| Event chain | Open→Headers (policy point)→Body (streaming fragments)→Request→Error/Close; Worker serial |
| Headers five policies | buffer/stream/discard/reject/respond directly; caps at routing level |
| Body backpressure | Pause only inside the callback, expires per fragment; Resume from any thread, zero allocation; timeout counts during the pause |
| Response uniqueness | one final commit; Inform sends 1xx; three paths: Reply/ReplyBody/Respond |
| Direct-answer path | Reply, zero containers, straight into the frozen kernel; the body reference may be destroyed after submission |
| Lifecycle | Drain drains; Shutdown published after all reclamation; spin on Closed |
| Network | multi-endpoint/dynamic port/reuse-port fully exposed; Network lends the underlying reference |
| Trimming | Server = Exchange + Response + TCP Server; streaming/file/TLS/Upgrade independent layers |
