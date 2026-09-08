---
num: 98
slug: xhttp-runtime
title: The xhttp Runtime: Builders, Calls, and the Connection Pool
volume: 卷十 扩展库：xhttp
type: practice
lead: The freeze semantics of request snapshots, two kinds of deadlines with cancellation, diagnostic snapshots, and the origin-sharded connection pool — all the runtime's gears.
api: xhttp-http_client, xhttp-http_client_runtime, net
---

## Orientation

What did the easy layer (Chapter 97) fold away? This chapter unfolds it.**The request builder** (`xhttprequest`): explicit assembly of method/URL/Headers/body/auth; `Do` freezes a snapshot at submission — the caller may modify or destroy the original request immediately;**the call layer**: `xhttpcalloptions`' two kinds of deadlines (a total timeout covering the whole chain, an idle timeout watching only time without progress), cancellation tokens, and response-body caps;**diagnostics**: `xhttpcallresult.Info`'s state machine and timestamps (monotonic-clock microseconds) — `RequestWireBytes`/`ResponseWireBytes` accumulate across the entire redirect chain;**the connection pool**: sharded by origin (at most 32 shards), each with its own waiting FIFO and idle LRU, a global atomic quota — reuse is the root of performance. Four gears together are "the runtime" — every easy line runs on top of them.

## Introduction

Three scenes push you down from easy. Scene one: sending a JSON POST with an `Authorization` header — three builder lines; easy can't (deliberately). Scene two: batch requests to the same service — the per-request TCP+TLS handshake cost is the throughput ceiling; pool reuse drops the cost of the second request onward to zero handshakes. Scene three: a slow origin — a total timeout governs "how long the whole request may wait", but an origin "pushing one byte per hour" never finishes within the total; the **idle timeout** exists precisely for this "connection alive but no progress" drag.

The third scene deserves one more thought: why two timeouts? A total timeout is a resource cap (this request may occupy at most 30 seconds of mine); an idle timeout is **progress detection** (keep waiting while the transfer moves; give up after 10 seconds stuck). Receiving request bytes and consuming response bytes both refresh the idle clock — "slow but moving" and "fast but stuck" become distinguishable. xhttp makes the two orthogonal configuration: `Timeout` and `IdleTimeout` independent, zero inherits from Client, `XHTTP_CLIENT_TIMEOUT_NONE` disables explicitly.

## Concepts

### The request builder and freeze semantics

`xrtHttpRequestCreate(方法, URL)` (method, URL) builds a modifiable request; `SetHeader`/`SetBytes` (fixed byte body, copied once)/`SetBody` (referenced body — files, producer streams)/`SetAuth` family (auth) assemble item by item.**Freeze semantics**: `xrtHttpClientDo` copies the request and value-semantic configuration and retains body references before returning success — **after submission, the original request may be modified or destroyed at once** (asynchronous execution's independence is guaranteed by the snapshot). `Clone`'s method/URL/Headers/Trailers are fully independent; the body only gains a reference — the template cost of "copy one, change one, send one" is O(delta).

### The call layer: deadlines, cancellation, and caps

```diagram flow
- Submission: Do freezes the snapshot -> queue -> a Worker executes -> DNS/dial/TLS/send/receive
- Total timeout: covers the whole chain from submission (queue/DNS/TCP/proxy/TLS/send-receive)
- Idle timeout: counts "no progress" from execution - transfer/send-receive bytes all refresh
- Cancellation: xrtHttpCallCancel on any thread; already-cancelled before submission goes straight to the cancelled final state
- Caps: ResponseBodyLimit bounds the representation body (pre-decode); Decompress.MaxBody bounds the plaintext
```

The layering of the two caps must be kept clear: `ResponseBodyLimit` acts on the representation body **after HTTP/1 frame wrapping is stripped and before Content-Encoding decoding** — network responses, cache hits, and locally composed Range responses share one budget; the decoded plaintext is separately capped by `Decompress.MaxBody` — **highly compressed content cannot get around the memory budget** (the client edition of Chapter 96's "bomb defense"). Long-lived connections like SSE choose an unbounded representation body and rely on the Parser's structured caps — "what to cap" is decided by content structure, not one-size-fits-all.

### The diagnostic snapshot: every Info field

`xhttpcallresult.Info` (carried by the completion callback; a running snapshot may also be copied from any thread): `State` (queued/executing/**immutable final state** — once the final state is published, every field freezes; a late cancellation cannot rewrite a published result), `Phase` (the actual ending phase retained), microsecond timestamps (`TransportReady`/`RequestSent`/`FirstByte`/`Headers` — monotonic clock, zero if not reached), byte accounting (`RequestWireBytes`/`ResponseWireBytes` **accumulate across the entire redirect chain**; `ResponseBodyBytes` is the finally delivered body — plaintext when auto-decompressed), `ReusedConnection` (any hop reused), `Secure` (current/final hop encryption). This field set is the "which hop is slow" checkup sheet — Chapter 136's performance analysis will return to use it.

### The connection pool: origin shards and fair waiting

With `http_client_pool` enabled, connections are managed per origin (scheme+case-insensitive host+port+proxy identity):

- **Sharding**: at most 32 fixed origin fragment shards by Worker count, each with its own index/waiting FIFO/idle LRU/sweeper Timer — **no Timer per connection**; across shards only an atomic quota coordinates, no Client-level hot lock.
- **Fairness**: waiting within a shard runs "earliest runnable" FIFO; when a connection is returned, a same-origin waiter takes it over directly; when the global quota is exhausted, distribution rotates across shards — no "some shard sleeps forever".
- **Return conditions**: message boundary complete, no `Connection: close`, no upgrade, transport healthy — all four pass before reuse;**no HTTP/1 pipelining** (one transaction at a time per connection — Chapter 89's smuggling defense, engineered).
- **Defaults**: active unlimited, global idle 128, per-origin idle 8, 90-second sweep; an idle cap of zero disables reuse.
- **Operations**: `xrtHttpClientCloseIdle` (removes only idle, interrupts nothing active), `xrtHttpClientStats` (concurrently readable gauges and monotonic counters).

### Validate at submission, freeze on failure

Client creation validates static configuration; `Do` submission validates configuration ranges (address wraparound rejected synchronously, no callback published); once a final state is published it is immutable — **late cancellation/timeout/transport failure cannot overwrite a published result**. These three pin "configuration error" and "runtime failure" to well-defined moments, so errors stop drifting during debugging.

## Examples

### First complete program: builder assembly and URL introspection

The program below is from `examples/http/client_request` — the explicit assembly of a JSON POST:

```embed path="extlibs/xhttp/examples/http/client_request/main.c" title="extlibs/xhttp/examples/http/client_request/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/client_request/main.c -lws2_32 -liphlpapi
POST api.example.test:443
```

**What just happened.** (1) `HttpRequestCreate(方法, URL)` (method, URL) builds the request in one step — the URL is full-form (scheme+host+query), parsed internally by Chapter 101's `xurl`. (2) `SetHeader` adds Accept, `SetBytes` copies the 14-byte JSON body and declares the type — the builder's two everyday Sets. (3) `xrtHttpRequestUrl` lends out the parsed URL structure and `xrtUrlPort` takes the numeric port (443 — HTTPS's default filled in here, Chapter 101's `XURL_PORT_VALUE` semantics); the print proves **builder introspection**: method, host, and port are all readable facts. (4) Destroy closes — an unsubmitted request still works as a "structured holder of URL/method". Real submission: after `xrtHttpClientDo(pClient, pRequest, NULL, on_done, pCtx)` **Destroy the original request immediately** — freeze semantics spare you tracking "is async still using it?".

### Second complete program: pool statistics and idle sweeping

The second program is from `examples/http/client_pool` — the runtime's operational face:

```embed path="extlibs/xhttp/examples/http/client_pool/main.c" title="extlibs/xhttp/examples/http/client_pool/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/client_pool/main.c -lws2_32 -liphlpapi
（示例校验连接池统计与空闲清理路径后正常退出）
```

**What just happened.** (1) `HttpClientStats(pClient, &Stats)` reads the pool's current gauges and monotonic lifecycle counters — **the fields do not promise one global instant** (loose consistency under concurrent reads); `ActiveConnections` includes dialing/in-use/quota-reserved connections. (2) `HttpClientCloseIdle` removes only idle — interrupting no active call; the lifecycle final state additionally waits for asynchronous Closes and shard Timer cancellation callbacks ("removed from LRU" ≠ "fully closed" — distinguishing the two moments is this API's honesty). (3) The configuration face: `xhttpclientpoolconfig`'s idle/waiting/active caps are deployment parameters — 8 idle per origin suffices for most API clients; services with constrained upstream connections lower the global cap. Stats → adjust → stats again is the capacity loop.

## Contracts

- **Freeze semantics**: `Do` completes the copy before returning success; after submission the original request is modifiable and destroyable; `Clone` copies by delta (body gains a reference).
- **Two deadlines**: `Timeout` spans the whole chain from submission; `IdleTimeout` measures no-progress from execution (send/receive refresh); zero inherits from Client, `TIMEOUT_NONE` disables; simultaneous expiry reliably reports TOTAL.
- **Cancellation**: `CallCancel` from any thread; already-cancelled before submission installs no Timer and starts no DNS, going straight to the final state; a published final state is immutable.
- **Dual caps**: `ResponseBodyLimit` (representation body, pre-decode, cache shares the budget) + `Decompress.MaxBody` (plaintext) — high compression cannot bypass the budget; SSE replaces the download cap with structured caps.
- **Info freeze**: final-state fields frozen; timestamps monotonic-clock microseconds, zero if unreached; Wire fields accumulate the redirect chain; Body records the final plaintext.
- **Pool sharding**: ≤32 fixed shards; per-shard FIFO/LRU/Timer; cross-shard atomic quota; four return conditions; no pipelining; defaults 128/8/90s.
- **Pool operations**: CloseIdle interrupts nothing active; Stats loosely consistent, concurrently readable; Drain/Abort share one path and wait for asynchronous teardown.
- **Submission validation**: configuration-range wraparound rejected synchronously without publishing a callback; creation validates the Dial/Stream/Exchange static configuration.
- **Trimming**: `HTTP_CLIENT_POOL`/`_FUTURE`/`_PREPARE`/`_STREAM` independent — trim to the faces you use.

## Pitfalls

### Pitfall 1: modifying the original request after submission, expecting to affect the in-flight call

Symptom: after `Do`, another `SetHeader` — the in-flight request "doesn't carry" the new header, suspected library bug.

Cause: freeze semantics — the snapshot was fixed the moment `Do` returned success; later modifications belong to "the next submission". This is a feature, not a defect: otherwise every submission would have to wait for the caller to announce "done editing".

```c bad
pCall = xrtHttpClientDo(pClient, pReq, NULL, on_done, pCtx);
xrtHttpRequestSetHeader(pReq, Extra, Value);  /* can't touch the in-flight one */
```

```c good
pCall = xrtHttpClientDo(pClient, pReq, NULL, on_done, pCtx);
xrtHttpRequestDestroy(pReq);   /* the original request's mission is over */
/* need more? build a new request and submit again - snapshot semantics keep concurrency independent */
```

### Pitfall 2: only a total timeout — a slow-drip connection drains the budget

Symptom: total timeout 30 seconds, origin pushes one byte every 20 seconds — every request "legally" burns the full 30 seconds; throughput is dragged down by such origins.

Cause: the total timeout watches duration, not progress. The idle timeout is the progress detector — "connection alive but not moving" should be abandoned after 10 seconds.

```c bad
xrtHttpClientConfigInit(&Config);
Config.Timeout = 30000000ull;   /* total only: one byte per 20s drains the whole span */
```

```c good
xrtHttpClientConfigInit(&Config);
Config.Timeout = 30000000ull;        /* total budget 30s */
Config.IdleTimeout = 10000000ull;    /* abandon after 10s without progress */
/* transfer/send-receive refresh idle - "slow but moving" unaffected, "stuck" cut short fast */
```

### Pitfall 3: treating Stats as a precise snapshot for capacity decisions

Symptom: dividing `ActiveConnections + IdleConnections` against instantaneous QPS yields conclusions that keep drifting.

Cause: Stats fields **do not promise one instant** — under concurrent reads each field comes from a different moment. Capacity decisions should use the **monotonic lifecycle counters** (cumulative created/destroyed/reused) to compute trends, not ratios of instantaneous gauges.

```c bad
double load = (double)Stats.ActiveConnections /
	(Stats.ActiveConnections + Stats.IdleConnections);  /* instantaneous mix: drift */
```

```c good
/* lifecycle counters are monotonic: two samples give rates/reuse rate - stable and comparable */
double reuse_rate = (double)Stats.ReusedConnections /
	(double)(Stats.CreatedConnections + 1);
```

## Exercises

### Basic: full-element builder assembly

Send a POST with an `Authorization` header and JSON body via the builder (against a local or httpbin service), printing status and `Info.Phase` in the completion callback. Acceptance criteria: destroying the original request right after submission doesn't affect the result; the response body and headers are fully readable.

### Advanced: the dual-timeout experiment

Stand up a slow service (one byte per second) and a stuck service (accepts connections, sends nothing); measure abandonment times under total-only, idle-only, and both. Acceptance criteria: the three abandonment times match contract derivation; you can explain the `Info.Phase` differences.

### Challenge: a connection-reuse benchmark

Fire 100 requests at the same origin (concurrency 8); from Stats' lifecycle counters compute the reuse rate and handshake count; then disable the pool via `ClientConfig` (idle cap zero) and compare. Acceptance criteria: reuse rate, total time, and `Info.ReusedConnection` hit rate are three self-consistent datasets; you can explain where the time difference with the pool off comes from (TCP+TLS handshakes × count).

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Builder | Create(method,URL) + SetHeader/SetBytes/SetBody/SetAuth family; Do freezes the snapshot |
| Freeze semantics | after submission the original is modifiable/destroyable; Clone by delta (body gains a reference) |
| Two deadlines | Timeout whole chain (from submission); IdleTimeout no-progress (send/receive refresh); zero inherits/NONE disables |
| Cancellation | any thread; pre-submission cancellation goes straight to the final state; the published final state is immutable |
| Dual caps | representation body (pre-decode, cache shares the budget) + plaintext (post-decompression) - two gates against bombs |
| Info | final state frozen; microsecond monotonic clock; Wire accumulates the redirect chain; Body records plaintext bytes |
| Pool sharding | ≤32 fixed shards; per-shard FIFO/LRU/Timer; cross-shard atomic quota, no hot lock |
| Return conditions | boundary complete + no close + no upgrade + healthy; no pipelining; defaults 128/8/90s |
| Operations | CloseIdle interrupts nothing active; Stats loosely consistent - capacity reads the monotonic counters |
| Introspection | RequestUrl/Method readable; UrlPort fills default ports - the request is also a structured holder |
