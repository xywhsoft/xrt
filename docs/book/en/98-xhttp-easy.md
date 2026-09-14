---
num: 98
slug: xhttp-easy
title: xhttp easy: One-Line Requests at Full Power
volume: 卷十 扩展库：xhttp
type: practice
lead: Behind the one-line GetSync there is no second implementation — how the convenience layer transparently inherits the runtime's timeouts, caching, TLS, and error chain.
api: xhttp-http_client, xhttp-http_client_easy, net
---

## Orientation

Volume 9 gave you every HTTP part (parsing, framing, fields, decoding); Volume 10's xhttp assembles them into a **high-level client** — this chapter starts at the thinnest layer: the **easy convenience layer**. `xrtHttpClientGetSync(客户端, URL, NULL)` (client, URL, NULL) completes request-response in one line; the key is that behind this line there is **no hidden Engine, Client, or second request implementation** — it merely creates a temporary request and hands it to `xrtHttpClientDo()` to freeze; timeouts, cancellation, redirects, cookies, caching, proxies, TLS, decompression, diagnostics, and the error chain are all identical to the low-level entrance. After this chapter you hold "the shortest usable path" plus a judgment: when to stay at the easy layer and when to sink to Chapter 99's request builder.

## Introduction

Writing a "fetch a URL and print the status" tool is one command in the curl era; in the C-library era it is usually thirty lines of assembly (Engine, Client, request, callback, wait, cleanup). The easy layer compresses this to three core lines — but the design question lies beyond "compression": most libraries' convenience layers are **a second code path**, quietly diverging from the low-level entrance (easy doesn't support redirects, error codes differ, diagnostics missing). xhttp's stance is that the convenience layer is only syntactic sugar: `http_client_easy` creates a temporary `xhttprequest` for the duration of the call, hands it to `Do` to freeze, and destroys it before returning — **you can always mechanically rewrite an easy call into builder form with unchanged behavior**.

This layer's existence makes xhttp's learning curve a three-step staircase: this chapter (one-line calls) → Chapter 99 (builders and the runtime panorama) → Chapter 104 (the server).

## Concepts

### Three convenience entrances and three completion shapes

| Entrance | Semantics |
| --- | --- |
| `xrtHttpClientGet(客户端, URL, 选项, 回调, 上下文)` (client, URL, options, callback, context) | body-less GET (**never fabricates an empty body**) |
| `xrtHttpClientPost(客户端, URL, 字节, 类型, 选项, 回调, 上下文)` (client, URL, bytes, type, options, callback, context) | copies a fixed byte body |
| `xrtHttpClientSendBytes(...)` | PUT/PATCH/any method carrying a fixed byte body |

The callback shape is the baseline; enabling `http_client_easy_future` adds `Async` (returning an `xfuture`) and `Sync` (blocking for an `xhttpresult*`) suffixes to the same entrances.**Sync entrances must never block on a network Worker** — they await the final state on the calling thread; calling them on a Worker deadlocks — the same discipline as Chapter 82's synchronous Future waiting.

### The semantic divide: GET without a body vs POST with an empty body

The convenience layer deliberately distinguishes two "nothing heres": a GET **has no body** (no Content-Length/Content-Type generated); POST/SendBytes given an **empty byte view means an explicit zero-length body** (generating `Content-Length: 0`). The distinction comes from protocol semantics — a GET's body is undefined, while a POST empty body is a legal and explicit statement. Likewise, an empty `ContentType` generates no Content-Type — no lying.

### The convenience layer's boundary: what it deliberately does not accept

Headers, streaming bodies, forms, auth, upload — the convenience entrances **deliberately refuse** these. When you need them, sink: `xrtHttpRequestCreate()` + `xrtHttpRequestSetBody()` + `xrtHttpClientDo*()` — the low-level entrances stay public alongside the convenience layer (Chapter 99). This "deliberately" is design: the convenience layer's parameter table stays one-line sized; complex needs are expressed with the explicit builder — there is no "hidden config struct of the convenience layer" middle state.

### The result object: a read-only snapshot

`xhttpresult` is the read-only snapshot handed to the caller on completion: `xrtHttpResultResponse(结果)` (result) lends out an `xhttpresponse` (version/status/reason/Headers/trailers/**the final effective URL** — the landing point after redirects, Chapter 100), `xrtHttpResponseBody(响应)` (response) a contiguous body view, `xrtHttpResultDestroy` frees. The body grows on demand without a reserved fixed buffer; streaming execution (Chapter 99) allocates no body buffer and records only byte counts.**The result must be Destroyed when used up** — it is this chapter's only owning object you manage.

### The minimal assembly

Three fixed steps before any easy call: `xrtNetEngineConfigInit/Create/Start` (Engine) → `xrtHttpClientConfigInit` + `xrtHttpClientCreate(引擎, 配置)` (engine, config) (Client) → the convenience call. Client creation validates the static configuration of Dial/Stream/Exchange/private Resolver on the spot — an invalid strategy fails immediately, not deferred to the first request (the same philosophy as Chapter 83's identity "validate at startup"). Cleanup order matches Chapter 67: Client first, then Engine; the Engine's Destroy may need to wait for Workers to drain (spin-retry is the standard posture — this chapter's sample's teardown is the template).

### easy's place in xhttp's layering

```diagram flow
- easy convenience layer (this chapter): Get/Post/SendBytes one line - a temporary request handed to Do to freeze
- Runtime (Chapter 99): request builder + Client + call options + connection pool/timeouts/cancellation
- Automatic-behavior layer (Chapters 100/101): redirects/retries/cookies/decompression/caching - config switches
- Protocol foundation (Volume 9): parsing/framing/fields/decoding - the parts
```

## Examples

### First complete program: a one-line GET

The program below is from `examples/http/client_easy` — the complete tool of three core lines:

```embed path="extlibs/xhttp/examples/http/client_easy/main.c" title="extlibs/xhttp/examples/http/client_easy/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/client_easy/main.c -lws2_32 -liphlpapi
usage: client_easy <http-url>
```

**What just happened.** (1) After the three-step assembly, `xrtHttpClientGetSync(pClient, URL视图, NULL)` (URL view, NULL) submits and waits in one line — the third parameter `NULL` takes the default call options (timeouts/redirects/caching etc. inherit the Client configuration). (2) The success path's trio: `HttpResultResponse` lends the response → `HttpResponseStatus` status code + `HttpResponseBody` body view (fwrite directly, zero copy) → `HttpResultDestroy` frees. (3) On failure `GetSync` returns `NULL` with details in the thread error slot (Chapter 4's model running through to here). (4) The teardown's spin `while (!xrtNetEngineDestroy(...))` deserves note — Engine destruction may fail over in-flight draining; clearing the error and retrying until success is the standard xhttp spelling of "destroy the Engine last".

### Second complete program: the Future shape

The second program is from `examples/http/client_future` — the asynchronous face of the same calls:

```embed path="extlibs/xhttp/examples/http/client_future/main.c" title="extlibs/xhttp/examples/http/client_future/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/client_future/main.c -lws2_32 -liphlpapi
usage: client_future <http-url>
```

**What just happened.** (1) This one submits via the builder path (`HttpRequestCreate` + `HttpClientDo`), because the demonstration target includes the request object's explicit lifecycle — against easy: the `Get` family is exactly this three-line sequence folded. (2) After the Future completes, `xrtFutureValue` takes the result and `HttpResultResponse` walks the same read-only-snapshot path — **Sync and Future share one result object**; the convenience layer differs only in "who waits" (Chapter 82's dual shapes, xhttp edition). (3) Read the two samples side by side: same assembly, same teardown, same error model — the difference is only the two lines of submit and wait. That is the verifiable shape of "no second implementation".

## Contracts

- **No second implementation**: easy only creates a temporary request handed to `Do` to freeze; every behavior (timeouts/cancellation/redirects/cookies/caching/proxy/TLS/decompression/diagnostics/error chain) matches the low level; mechanically rewritable into builder form.
- **Three entrances, three shapes**: Get/Post/SendBytes × callback/Async/Sync; Sync must never block on a Worker.
- **Body semantics**: GET has no body (no fabricated empty body); an empty byte view = explicit zero length; an empty ContentType = the field is not generated.
- **Deliberate refusals**: Header/streaming/forms/auth — sink to the builder (Chapter 99); no middle state.
- **Result object**: read-only snapshot; `ResultResponse` lends the response, `ResponseBody` lends the body; the final effective URL includes the redirect landing point; `ResultDestroy` when used up.
- **Startup validation**: Client creation validates the static configuration; invalid strategies are not deferred to the first request.
- **Trimming**: `XHTTP_MODULE_HTTP_CLIENT_EASY` (callback) and `_EASY_FUTURE` (Future/sync) independent.
- **Assembly discipline**: Engine started first; cleanup Client before Engine; Engine Destroy spins and retries.

## Pitfalls

### Pitfall 1: calling a Sync entrance inside a Worker callback

Symptom: deadlock — the completion callback runs on a Worker, and Sync waits for completion on the same thread.

Cause: Sync's implementation is "the calling thread awaits the final state"; a Worker waiting on work it must itself complete is a mutual wait forever. Inside a callback, either forward to another thread or just use the callback shape itself.

```c bad
static void on_done(xhttpcall* pCall, ..., ptr pCtx) {
	/* on a Worker: */
	pResult = xrtHttpClientGetSync(pClient, OtherUrl, NULL); /* deadlock */
}
```

```c good
static void on_done(xhttpcall* pCall, ..., ptr pCtx) {
	/* in a callback, submit the next request with the callback shape */
	xrtHttpClientGet(pClient, OtherUrl, NULL, on_next, pCtx);
}
/* truly need serial blocking waits: use Sync/Future on a non-Worker thread */
```

### Pitfall 2: treating easy as a capability boundary and detouring to build config

Symptom: needing one Header, you abandon easy — hand-assembling Engine+DNS+TCP+TLS to rewrite the request path; or conversely, jamming a global config struct into easy's parameters.

Cause: missing that the boundary is deliberate: there is no hidden config above easy; what you need is not "a smarter easy" but **sinking one layer** — the builder's one `HttpRequestSetHeader` is enough, and the whole assembly is reused.

```c bad
/* dropping the whole xhttp client for one Header */
hand_roll_engine_dns_tls_http();  /* 300 lines */
```

```c good
xhttprequest* pReq = xrtHttpRequestCreate();
xrtHttpRequestSetMethod(pReq, ...);
xrtHttpRequestSetUrl(pReq, ...);
xrtHttpRequestSetHeader(pReq, XRT_STR_LITERAL("Authorization"), Token);
xrtHttpRequestSetBytes(pReq, Body, iSize, Type);
pCall = xrtHttpClientDo(pClient, pReq, NULL, on_done, pCtx);
xrtHttpRequestDestroy(pReq);   /* after Do freezes the snapshot, the original request can be destroyed at once */
```

### Pitfall 3: forgetting to Destroy the result (or Destroying the response early)

Symptom: one result object leaked per request; or the body view used halfway when the response is freed — crash.

Cause: `xhttpresult` is the owning snapshot (response/body belong to it); `ResultResponse`/`ResponseBody` are only borrows. The lifetime rule: **the views live in the result; the result lives until your Destroy**.

```c bad
const xhttpresponse* pR = xrtHttpResultResponse(pResult);
xrtHttpResultDestroy(pResult);      /* destroyed the home first */
printf("%.*s\n", Body from pR);     /* the view dangles */
```

```c good
const xhttpresponse* pR = xrtHttpResultResponse(pResult);
xbytesview Body = xrtHttpResponseBody(pR);
consume(Body);                       /* consume first */
xrtHttpResultDestroy(pResult);       /* release after */
```

## Exercises

### Basic: run each of the three entrances

Against the same test service (a local Chapter 104 server sample or any httpbin-like service), fetch results with Get/Post/SendBytes respectively, printing status code and body length. Acceptance criteria: the assembly code of the three entrances is identical; the GET response generates no Content-Type request header.

### Advanced: Sync vs Future side-by-side

Complete the same URL once with `GetSync` and once with `Get`+Future; use `xhttpcallresult.Info`'s `FirstByte`/`RequestWireBytes` to compare the diagnostics of the two calls. Acceptance criteria: the diagnostic fields are semantically identical across paths; you can explain the difference between `WireBytes` and `BodyBytes` (on-the-wire bytes vs delivered body — different after decompression).

### Challenge: mechanical easy→builder rewrite

Rewrite a `Post` (with type) call line by line into `HttpRequestCreate/SetMethodUrl/SetBytes + Do` form; compare outputs and diagnostics of both versions. Then rewrite `Get`. Acceptance criteria: behavior item-by-item identical (status/body/final URL/error category); the rewrite needs no "easy-specific" documentation — because none exists.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Design stance | no second implementation: a temporary request handed to Do to freeze; behavior fully consistent with the low level |
| Three entrances | Get (no body) / Post (copies bytes) / SendBytes (any method + bytes) |
| Three shapes | callback baseline; `_EASY_FUTURE` adds Async (xfuture) / Sync (blocking) |
| Body semantics | GET no body; empty view = explicit zero length; empty type = field not generated |
| Boundary | deliberately refuses Header/streaming/forms/auth - sink to the builder |
| Result object | read-only snapshot; Response/Body borrowed; final URL includes the redirect landing; Destroy mandatory |
| Sync discipline | never block on a Worker - use the callback shape inside callbacks |
| Startup validation | Client creation validates the static configuration |
| Assembly | Engine start → Client create → call; cleanup in reverse, Engine spins to destroy |
| Trimming | EASY and EASY_FUTURE independent macros |
