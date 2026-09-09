---
num: 104
slug: xhttp-middleware
title: The Server (Part 2): Middleware and Static Files
volume: 卷十 扩展库：xhttp
type: practice
lead: The onion model entering in registration order and exiting in reverse, Next's synchronous-stack semantics, static files and directory serving — the standard assembly of cross-cutting concerns.
api: xhttp-http_server_middleware, xhttp-http_server_static, xhttp-http_server_router
---

## Orientation

Everything outside business routing — logging, timing, auth, CORS, compression — must run for every request. `xrtHttpServerUse` registers them as **middleware**: entered in registration order, exited in reverse (the onion model); `xrtHttpServerNext` enters the next layer synchronously, and post-logic (timing/log finalization) runs after it returns;**not calling Next is a short-circuit** — middleware may respond directly or retain the Connection for async handling. The contract's key detail: `Next` is valid only within the current synchronous callback stack (never store it into a thread/Future/coroutine) and may be called at most once (repetition = error path) — **async middleware should short-circuit and retain the Connection itself, not delay Next**. The chapter's second half covers the static file layer: single-file responses, directory mapping with sandbox constraints (`http_server_static` — Chapter 46's dir-sandbox idea landing server-side).

## Introduction

The onion model's value lies in paired "before + after": a logging middleware notes the start, calls Next, and on return notes the end and status — one layer of code covering the request's whole lifetime. But asynchrony breaks the naive onion: if middleware wants to "check a cache before letting through" (an async lookup), when is Next called? The contract's answer is **don't** — short-circuit and retain the Connection; after the async completes, respond yourself or dispatch manually to the final route. This discipline comes from a technical constraint: `Next` is a handle on the synchronous stack frame — saving it elsewhere and calling it re-enters the state machine (the same prohibition as Chapter 85's Select callbacks).

Static files are another cross-cutting kind: nearly every service must "send a file" — favicon, frontend build artifacts, download archives. Hand-writing "read the file + guess the type + answer Range" is every C server's traditional routine; `http_server_static` standardizes it: single files, directory prefix mapping, and Range negotiation (Chapter 90's Range in server form) all in one place.

## Concepts

### Registration and the onion

```diagram flow
- Registration: Use(middleware A) -> Use(B) -> Get(route) (middleware before routes) -> RouterFreeze
- Request: A's before -> B's before -> route handler -> B's after -> A's after (reverse exit)
- Next: enter the next layer synchronously; return = the next layer's dispatch has returned (not I/O completion)
- Short-circuit: no Next call - middleware responds directly or retains the Connection async
```

Two registrations: `Use` (the caller manages the data) and `UseOwned` (after successful registration, the `Release(Data)` responsibility moves to the Router — the last reference's destruction releases in reverse registration order).**Callback return value**: `true` = this layer accepts the request; `false` = an unrecoverable application error — the runtime best-effort commits a fixed 500, or closes abnormally if a response was already committed. The middleware count is bounded by the Router's `MaxRoutes` cap; registration growth is atomic (failure changes no visible count and transfers no cleanup responsibility).

### Next's exact semantics

Three easily misunderstood points.① **Next returning ≠ response written**: the return only means the next layer's dispatch returned — a synchronous direct answer has usually entered the `XHTTP_CONN_RESPONSE` state, while an async route may still be waiting; to time full responses use the Connection's response-completion path.② **Valid within the stack**: `xhttpservernext` is valid only within the current synchronous callback stack — saving it to another thread/Future/coroutine and calling is undefined behavior; repeated calls set `XHTTP_SERVER_MIDDLEWARE_ERROR_NEXT` and enter the failure path.③ **Parameter pass-through**: on a matched route, middleware receive the same `Params/Count` the final route sees (empty on no match) — middleware can see route parameters (the id in your logs).

### Cooperating with 404/405/OPTIONS

Unified logging and error-handling middleware must be able to see 404/405 and automatic OPTIONS — with at least one middleware enabled, **the body of an unmatched request is discarded first and then the request enters the full chain** (the logging layer sees the full request shape); with no middleware, an unmatched request responds directly at the Headers stage and the body is rejected (the convenient fast path). Enabling changes the "handling depth of the unmatched" — precisely the delivery of "middleware takes over the cross-cutting".

### The static file layer

`http_server_static` maps files to routes:

- **Single file**: the `xrtHttpConnFile` family — one path to one file (favicon, robots, and friends); the Range variant `xrtHttpConnFileRange` at the same level.
- **Directory mapping**: URL prefix ↔ disk directory — `/static/*` mapped to the build-artifacts directory; traversal protection (`../` attacks intercepted at the mapping layer — Chapter 46's dir-sandbox idea).
- **Negotiation**: Content-Type by extension (MIME database), ETag/Last-Modified with conditional requests (the 304 path), **Range requests** (the server-side mirror of `http_cache_range` — resume support).
- **Response path**: file responses use `ReplyBody`'s file body source (Chapter 103's direct-answer path) — zero-copy sending (the kernel-direct SendFile family from Chapter 67 honored at the transport layer).

### Unified dispatch path

Fixed Routers and the Host Mux (a multiplexer branching on the Host header — one service, many domains) share **one dispatch path** — the middleware table and route table freeze together into read-only shared form. Lock-free dispatch on the hot path (the same performance philosophy as Chapter 98's pool sharding). The route pattern's `{name}` segment-capture syntax (`http_route.h`) shares its idea with the `{name}` capture of Chapter 30's cousin pattern — freeze is precisely where "compile once, look up only on the hot path" lands in the routing layer.

## Examples

### First complete program: middleware + route assembly

The program below is from `examples/http/server_middleware` — a logging middleware wrapping a health route:

```embed path="extlibs/xhttp/examples/http/server_middleware/main.c" title="extlibs/xhttp/examples/http/server_middleware/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/server_middleware/main.c -lws2_32 -liphlpapi
middleware: 1, routes: 1
```

**What just happened.** (1) Assembly order is onion order: `Use(日志中间件)` (logging middleware) registered first, `Get("/health", 路由)` (route) second, `RouterFreeze` freezes — **middleware always wraps routes** (registration order is wrapping order). (2) The logging middleware's shape template: before prints `> 方法 目标` (method target) -> `xrtHttpServerNext(pNext)` enters the next layer (the health route answering JSON) → after return prints `< state` — `xrtHttpConnState` shows the response already committed (the synchronous direct path). (3) Output `middleware: 1, routes: 1` echoes the assembly facts — `MiddlewareCount`/`RouterCount` are configuration introspection (the same "configuration verifiable" posture as Chapter 98's Stats). (4) In a real service this Router hangs into `xhttpserverconfig`'s dispatch configuration — this sample focuses on the assembly layer so no network.

### Second complete program: static file serving

The second program is from `examples/http/static_file` — the standard assembly of file responses:

```embed path="extlibs/xhttp/examples/http/static_file/main.c" title="extlibs/xhttp/examples/http/static_file/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I extlibs/xhttp/single -include xhttp.h impl.c extlibs/xhttp/examples/http/static_file/main.c -lws2_32 -liphlpapi
（输出静态服务装配结果并正常退出）
```

**What just happened.** (1) The static layer installs "path → file" mapping into the routing system — single-file registration (favicon-class) and directory prefix mapping (`/static/*`) have separate entrances; extension-to-MIME judgment built in. (2) File responses go through Chapter 103's `ReplyBody` file source — no reading into memory, zero-copy at the transport layer (a big-file service's memory shape independent of file size). (3) Conditional requests (If-None-Match/If-Modified-Since) and Range are negotiated inside the layer — 304 and 206 are automatic paths; `examples/http/static_path` (directory mapping and traversal protection), `static_multipart_body` (locally composed multipart responses), `server_file` (the running-service form) cover the static family's remaining faces.

## Contracts

- **Onion order**: registration order in, reverse out; middleware before routes; frozen read-only shared (fixed Router and Host Mux on one dispatch path).
- **Next semantics**: valid within the synchronous stack, at most once; return = dispatch returned ≠ I/O complete; repetition = error path.
- **Short-circuit**: not calling Next short-circuits — respond directly or retain the Connection async; async middleware must short-circuit, never delay Next.
- **Callback return**: true = accepted; false = unrecoverable error → best-effort 500 (abnormal close if already committed).
- **Parameter pass-through**: middleware see the same Params as the final route; descriptors and Next borrow the current stack only.
- **Unmatched depth**: with middleware, the unmatched body is discarded first then enters the chain (logs see 404/405/OPTIONS); without, direct answer at the Headers stage.
- **Caps and atomicity**: middleware count bounded by MaxRoutes; registration growth atomic; UseOwned releases in reverse registration order.
- **Static layer**: single file/directory mapping/traversal protection/MIME/conditional requests/Range built in; file responses take the zero-copy path.
- **Trimming**: `HTTP_SERVER_MIDDLEWARE`/`HTTP_SERVER_STATIC`/`HTTP_SERVER_FILE` independent macros.

## Pitfalls

### Pitfall 1: saving Next and calling it async

Symptom: occasional state-machine scrambling or `MIDDLEWARE_ERROR_NEXT` — an async completion callback invoked a Next saved long before.

Cause: `xhttpservernext` is a handle on the synchronous stack frame. The correct async form is short-circuit + manage the connection yourself — not carrying a synchronous handle elsewhere.

```c bad
static bool onMiddleware(..., xhttpservernext* pNext, ...) {
	/* want to continue after an async cache lookup */
	g_SavedNext = pNext;               /* storing the handle: the stack frame is long dead */
	async_lookup(later_call_next);     /* calling later: undefined behavior */
}
```

```c good
static bool onMiddleware(xhttpserver* pS, xhttpconn* pC,
		const xhttpserverrequest* pReq, const xhttprouteparam* pP,
		size_t iP, xhttpservernext* pNext, ptr pD) {
	/* short-circuit: no Next call; retain the connection for the async decision */
	async_cache_lookup(pC, pReq);
	return true;   /* this layer accepts - the response is committed by the async path */
}
```

### Pitfall 2: timing the response at Next's return

Symptom: the logged durations are all sub-millisecond — because at Next's return the async route is still waiting; the "duration" measured only the dispatch stack.

Cause: Next returning = the next layer's dispatch returned; when the response bytes finish writing is another matter. Full timing must use the Connection's response-completion path (Close/response-drained events).

```c bad
uint64_t t0 = now();
Next(pNext);
log_duration(now() - t0);   /* measures stack time, not request time */
```

```c good
/* before: note the start (stored by request identity); pair the timing in the response-completion callback */
on_request_start(pConn);    /* record */
Next(pNext);
/* the completion moment reconciles on the response-completion path (e.g. the final state before Close) */
```

### Pitfall 3: forgetting the traversal boundary in static directory mapping

Symptom: `/static/../../etc/passwd`-style requests actually read system files — a hand-written mapping splices paths as strings with no normalization check.

Cause: paths must be verified "still inside the root after normalization" before splicing — precisely the protection `http_server_static`'s directory mapping builds in; going around the layer to splice paths goes around the protection too.

```c bad
snprintf(Path, "%s/%s", RootDir, UrlPath);   /* unchecked splice */
send_file(Path);                              /* ../ traversal goes straight through */
```

```c good
/* use the static layer's directory mapping: traversal protection/normalization built in */
/* or when self-managing: normalize first (UrlPathPathNormalize, Chapter 101) then verify the prefix */
if ( !path_inside_root(RootDir, Normalized) ) {
	reply_404();
}
```

## Exercises

### Basic: three-layer onion verification

Register logging, timing, and CORS middleware wrapping one route — print markers before and after each layer and observe the entry/exit order. Acceptance criteria: the output sequence strictly follows "registration order in, reverse out"; after any layer short-circuits, inner layers no longer execute.

### Advanced: an auth middleware

Implement an `auth_required` middleware: check the `Authorization` header; absent/invalid answers 401 directly (short-circuit); valid parses the token into the request context for the route. Cross-check with Chapter 105's auth scheme family. Acceptance criteria: protected routes 401 without a token, reachable with one; on middleware short-circuit the route does not execute.

### Challenge: a cached static service

Directory mapping + middleware negotiation: an ETag middleware (generate on first response, compare on conditional requests for 304) + the static layer's Range. Verify the three paths with curl (first 200, then 304, resume 206). Acceptance criteria: the three response statuses and bytes correct; 304 without body; Range fragments match file offsets.

## Cheat Sheet

| Topic | Quick reference |
| --- | --- |
| Onion model | registration order in, reverse out; middleware before routes; Freeze read-only shared |
| Next semantics | valid in the synchronous stack, at most once; return = dispatch returned ≠ I/O complete |
| Short-circuit | no Next call - respond directly or retain the connection async; async middleware must short-circuit |
| UseOwned | Release responsibility moves to the Router; last-reference destruction releases in reverse registration order |
| Callback return | true accepts / false unrecoverable → best-effort 500 |
| Parameter pass-through | middleware and routes see the same Params; descriptors borrow the current stack only |
| Unmatched depth | with middleware: body discarded first then into the chain; without: direct answer at the Headers stage |
| Static layer | single file/directory mapping/traversal protection/MIME/conditional requests/Range; zero-copy responses |
| Unified dispatch | fixed Router and Host Mux on one path; hot path lock-free |
| Trimming | MIDDLEWARE/STATIC/FILE independent macros |
