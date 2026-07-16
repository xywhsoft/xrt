# XHTTPD HTTP/1.1 Server Mainline

> The current HTTP/1.1 server mainline built on `xnet-v2 + xnet_stream + xtlssession`.

[Back to Index](README.en.md)

---

## 1. Positioning

`xhttpd` is the built-in HTTP/1.1 server layer in the current XRT mainline.

It is responsible for:

- listening and accepting connections
- parsing HTTP/1.1 requests
- materializing request / response objects
- supporting plain HTTP and builtin TLS paths
- serial keep-alive reuse on a single connection
- integrating with the runtime, future, task, and coroutine mainline

`xhttpd` is intentionally a transport-and-handler layer, not a full web framework.

Current boundaries:

- request bodies can be buffered or consumed through body-stream callbacks
- responses can be buffered, streamed, or sent from files
- routing and middleware are provided by the higher-level `xweb` layer
- PTY-style terminal interaction is unrelated to this layer

---

## 2. Core Types

### `xhttpdconfig`

Important fields:

- `iSize` / `iVersion`
- `tBindAddr`
- `iFlags`
- `iBacklog`
- `iRecvLimit`
- `pTlsConfig`

Always initialize this structure with `xrtHttpdConfigInit()`. `xhttpdconfig` and
`xhttpdevents` use a size/version prefix so a future runtime can accept an older
known prefix while defaulting newly added fields. Initialize event tables with
`xrtHttpdEventsInit()` instead of relying on a raw `memset`.

### `xhttpdrequest`

Materialized request object:

- request line fields: `sMethod`, `sTarget`, `sPath`, `sQuery`, `sVersion`
- dynamically growing header table: `arrHeaders / pHeaders`, with exact-length names and values
- body pointer and size: `pBody / iBodyLen`
- dynamically growing trailer table with exact-length names and values
- flags such as `XHTTPD_REQ_F_KEEPALIVE`

### `xhttpdresponse`

Materialized response object:

- status line fields: `iStatusCode`, `sReason`
- dynamically stored reason phrase without a fixed field-length ceiling
- dynamically growing header table: `arrHeaders / pHeaders`, with exact-length names and values
- body pointer and size: `pBody / iBodyLen`
- dynamically growing response trailer table
- close policy flags such as `XHTTPD_RESP_F_CLOSE`

### `xhttpdevents`

`xhttpd` now supports both synchronous and asynchronous request handling:

```c
typedef struct {
	uint32 iSize;
	uint32 iVersion;
	void (*OnOpen)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn);
	bool (*OnRequest)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, xhttpdresponse* pResp);
	xfuture* (*OnRequestAsync)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq);
	void (*OnClose)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, xnet_result iReason);
	void (*OnError)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, int iSysErr);
} xhttpdevents;
```

Handler priority:

1. If `OnRequestAsync` is set and returns a non-null future, that request becomes asynchronous.
2. If `OnRequestAsync` returns `NULL` and the connection has not already sent a response manually, `OnRequest` is used as the fallback.
3. If neither path handles the request, `xhttpd` generates `404 Not Found`.

---

## 3. Public API

### Request / Response Helpers

```c
XXAPI void xrtHttpdConfigInit(xhttpdconfig* pCfg);
XXAPI void xrtHttpdEventsInit(xhttpdevents* pEvents);
XXAPI void xrtHttpdRequestInit(xhttpdrequest* pReq);
XXAPI void xrtHttpdRequestUnit(xhttpdrequest* pReq);
XXAPI void xrtHttpdResponseInit(xhttpdresponse* pResp);
XXAPI void xrtHttpdResponseUnit(xhttpdresponse* pResp);
XXAPI xhttpdresponse* xrtHttpdResponseCreate(void);
XXAPI void xrtHttpdResponseDestroy(xhttpdresponse* pResp);
XXAPI void xrtHttpdResponseSetStatus(xhttpdresponse* pResp, uint32 iStatusCode, const char* sReason);
XXAPI bool xrtHttpdResponseSetHeader(xhttpdresponse* pResp, const char* sName, const char* sValue);
XXAPI bool xrtHttpdResponseAddHeader(xhttpdresponse* pResp, const char* sName, const char* sValue);
XXAPI bool xrtHttpdResponseSetTrailer(xhttpdresponse* pResp, const char* sName, const char* sValue);
XXAPI bool xrtHttpdResponseAddTrailer(xhttpdresponse* pResp, const char* sName, const char* sValue);
XXAPI bool xrtHttpdResponseSetBodyCopy(xhttpdresponse* pResp, const void* pData, size_t iLen, const char* sContentType);
XXAPI const char* xrtHttpdRequestHeader(const xhttpdrequest* pReq, const char* sName);
XXAPI const char* xrtHttpdRequestTrailer(const xhttpdrequest* pReq, const char* sName);
XXAPI uint32 xrtHttpdRequestTrailerCount(const xhttpdrequest* pReq);
XXAPI xhttpcontext* xrtHttpdRequestContext(const xhttpdrequest* pReq);
XXAPI const char* xrtHttpdResponseHeader(const xhttpdresponse* pResp, const char* sName);
XXAPI const char* xrtHttpdResponseTrailer(const xhttpdresponse* pResp, const char* sName);
XXAPI uint32 xrtHttpdResponseTrailerCount(const xhttpdresponse* pResp);
```

Request headers, request trailers, response headers, and response reason phrases are no longer constrained by the legacy per-field arrays represented by `XHTTPD_*_CAP`. `xrtHttpdRequestUnit()`, `xrtHttpdResponseUnit()`, and `xrtHttpdResponseDestroy()` release their dynamic strings. Do not copy initialized objects by value or mutate entries directly; use the public APIs.

### HTTP/1.1 validation and response semantics

The server rejects ambiguous framing, conflicting `Content-Length`, unsupported transfer codings, malformed chunks, invalid fields, and forbidden trailers before dispatching a handler. HTTP/1.1 requires exactly one valid `Host` authority; HTTP/1.0 accepts zero or one. A single valid `Expect: 100-continue` receives the interim response before the body is read, while unsupported or duplicate expectations receive `417 Expectation Failed`.

Response serialization enforces method and status semantics. `HEAD` sends the same representation metadata as `GET` but no body. `1xx` and `204` strip body framing and body bytes; `304` sends no body while preserving an explicitly supplied representation `Content-Length`. An empty chunked response emits exactly one terminal chunk.

Response trailers are owned copies and use chunked HTTP/1.1 framing. The serializer generates `Transfer-Encoding: chunked` and an exact `Trailer` declaration when needed. Trailer field names forbidden by HTTP, trailers on bodyless statuses or `HEAD`, and trailers combined with `Content-Length` are rejected. `xrtHttpdConnStart()` copies and prebuilds the terminal block, so the source response may be destroyed immediately after `Start` returns.

### Async Connection Helpers

```c
XXAPI bool xrtHttpdConnIsOpen(const xhttpdconn* pConn);
XXAPI xnet_result xrtHttpdConnRespond(xhttpdconn* pConn, const xhttpdresponse* pResp);
XXAPI xnet_result xrtHttpdConnStart(xhttpdconn* pConn, const xhttpdresponse* pResp);
XXAPI xnet_result xrtHttpdConnSend(xhttpdconn* pConn, const void* pData, size_t iLen);
XXAPI xnet_result xrtHttpdConnEnd(xhttpdconn* pConn);
XXAPI xnet_result xrtHttpdConnUpgrade(xhttpdconn* pConn, const xhttpdresponse* pResp,
    const xnetstreamevents* pEvents, ptr pUserData, xnetstream** ppStream);
XXAPI xnet_result xrtHttpdConnClose(xhttpdconn* pConn, uint32 iCloseFlags);
```

These helpers let asynchronous handlers send the response later from another task, thread, or coroutine-produced future.

`xrtHttpdConnUpgrade()` is the synchronous protocol handoff path. The response must be a bodyless `101` with valid `Connection: Upgrade` and `Upgrade` fields, and the request must itself be a bodyless Upgrade request. Call it only from the active synchronous handler on that stream's worker. On success, ownership of the returned stream transfers to the caller, the stream is removed from the HTTP server connection set, and HTTP no longer closes it during server shutdown. `OnOpen` is not called again. Bytes already received after the HTTP header are delivered to the new `OnRecv` after the handler returns. The caller must eventually close and destroy the stream.

At the xweb layer, `xrtWebResponseSetTrailer()`, `xrtWebResponseAddTrailer()`, the trailer accessors, and `xrtWebResponseUpgrade()` expose the same contracts without exposing `xhttpdconn`.

After an xweb response is committed by `Start`, sendfile, or Upgrade, response-building operations reject mutation. Status setters become no-ops and header, trailer, buffered-body, cookie, and redirect setters return failure. Middleware can still observe the committed response but cannot rewrite bytes already handed to the transport.

### Server Lifecycle

```c
XXAPI xhttpdserver* xrtHttpdCreate(xnetengine* pEngine, const xhttpdconfig* pCfg, const xhttpdevents* pEvents, ptr pUserData);
XXAPI uint16 xrtHttpdBoundPort(const xhttpdserver* pServer);
XXAPI xnet_result xrtHttpdStart(xhttpdserver* pServer);
XXAPI void xrtHttpdStop(xhttpdserver* pServer);
XXAPI void xrtHttpdDestroy(xhttpdserver* pServer);
```

---

## 4. Async Handler Contract

### `OnRequest`

`OnRequest` remains the simple synchronous path:

- fill `pResp`
- return `true` if handled
- return `false` to fall back to the built-in `404`

### `OnRequestAsync`

`OnRequestAsync` is the new request-hang-and-complete-later path.

Expected contract:

- return an `xfuture*` to mark the request as asynchronous
- return `NULL` to let `xhttpd` fall back to `OnRequest`
- if the future resolves successfully with a non-null value, that value must be an `xhttpdresponse*`
- if the future resolves successfully with `NULL`, it means the handler already responded manually through `xrtHttpdConnRespond()`
- if the future fails, `xhttpd` maps the failure to an HTTP error response such as `500`, `408`, or `503`

Ownership rules:

- ownership of the returned future is transferred to `xhttpd`
- ownership of the successful `xhttpdresponse*` result is also transferred to `xhttpd`
- `xrtHttpdResponseCreate()` / `xrtHttpdResponseDestroy()` are the recommended helpers for async response allocation

Request lifetime:

- the `xhttpdrequest*` remains valid until the async request is completed or the connection is torn down
- if you need request data after the async boundary, copying out the needed pieces is still the safest pattern
- `xrtHttpdRequestContext()` returns a borrowed context; retain it before asynchronous use
- that context is cancelled when the request finishes, the peer disconnects, or the server is stopped

Connection lifetime:

- `xhttpd` keeps the connection object alive while an async request is pending
- `xrtHttpdConnRespond()` is safe to call from another thread or task
- keep-alive requests stay serialized; the next request on the same connection is not advanced until the active async request is considered complete

---

## 5. Recommended Patterns

### Synchronous Handler

```c
static bool procOnRequest(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, xhttpdresponse* pResp)
{
	(void)pOwner;
	(void)pServer;
	(void)pConn;
	(void)pReq;
	xrtHttpdResponseSetStatus(pResp, 200u, "OK");
	xrtHttpdResponseSetBodyCopy(pResp, "hello", 5u, "text/plain; charset=utf-8");
	return true;
}
```

### Future Returns the Response

```c
static xfuture* procOnRequestAsync(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq)
{
	(void)pOwner;
	(void)pServer;
	(void)pConn;
	(void)pReq;
	return xTaskRunThread(procBuildResponse, NULL, 0);
}
```

Where `procBuildResponse()` returns `XRT_NET_OK` and sets `pOut->pValue` to an `xhttpdresponse*`.

### Manual Delayed Response

```c
static xfuture* procOnRequestAsync(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq)
{
	(void)pOwner;
	(void)pServer;
	(void)pReq;
	return xTaskRunThread(procRespondLater, pConn, 0);
}
```

Inside `procRespondLater()`:

- build an `xhttpdresponse*`
- call `xrtHttpdConnRespond(pConn, pResp)`
- destroy the local response object
- resolve the future with `NULL`

### Coroutine-Produced Future

`OnRequestAsync` can also return a future created by `xTaskRunCo()`. The common pattern is:

- create a scheduler
- create a coroutine future
- run the scheduler
- return the resulting future to `xhttpd`

This is useful when your service mainline already composes work around coroutines and future chaining.

---

## 6. Notes and Boundaries

- `xhttpd` does not turn into a routing framework automatically just because handlers are async.
- `xrtHttpdConnRespond()` sends a whole response; use `Start` / `Send` / `End` for streaming.
- `xrtHttpdConnUpgrade()` provides protocol-neutral stream handoff; `xws` builds WebSocket policy and framing on top of that contract.
- Routing, multipart form policy, negotiated response compression, and middleware belong to `xweb`.

### xweb middleware contract

```c
XXAPI bool xrtWebAppUseEx(xwebapp* pApp, xwebmiddleware pMiddleware,
    ptr pUserData, xwebfree pFreeUserData);
XXAPI bool xrtWebAppUse(xwebapp* pApp, xwebmiddleware pMiddleware, ptr pUserData);
XXAPI bool xrtWebServerUseEx(xwebserver* pServer, xwebmiddleware pMiddleware,
    ptr pUserData, xwebfree pFreeUserData);
XXAPI bool xrtWebServerUse(xwebserver* pServer, xwebmiddleware pMiddleware, ptr pUserData);
XXAPI xwebaction xrtWebNext(xwebnext* pNext);
```

Middleware runs in registration order around route, static-file, and built-in 404 dispatch. `xrtWebNext()` is synchronous, callback-scoped, and may be called at most once. A middleware that returns `XWEB_NEXT` without calling it is advanced automatically; after calling it, returning `XWEB_NEXT` preserves the downstream action. `XWEB_DONE` short-circuits and `XWEB_ERROR` discards an uncommitted partial response before generating 500. Each request sees the middleware count captured when dispatch begins, so concurrent registrations affect later requests only.

Generic middleware does not wrap streaming request-body callbacks. Streaming routes use their explicit `Begin / Body / End` lifecycle and must perform authentication, admission, and multipart storage policy in those callbacks. An uncommitted `XWEB_ASYNC` returned from the synchronous middleware/handler chain is rejected with 500; asynchronous service work belongs in the explicit future-based HTTPD path or an async streaming-body end handler.

### xweb multipart and compression policy

Buffered `multipart/form-data` requests are validated with the streaming multipart state machine before route dispatch. Missing or malformed boundaries, truncated input, invalid or duplicate semantic part fields, and parts without a form-data `name` return `400`. Part count, part body size, part header count, and part header bytes are bounded by the xweb configuration and return `413` when exceeded. Explicit streaming-body routes receive raw chunks and remain responsible for their own multipart storage policy.

Buffered xweb responses can opt into gzip and/or deflate with `iCompressionFlags`. `iCompressionMinBytes`, `iCompressionMaxBytes`, and `iCompressionLevel` bound CPU and memory cost. Negotiation honors `Accept-Encoding` q-values, wildcard and identity semantics, emits `Vary: Accept-Encoding`, returns `406` when no acceptable representation coding exists, and skips responses with `Cache-Control: no-transform`, `Content-Range`, an existing `Content-Encoding`, or a non-compressible media type. Streaming responses and sendfile paths are not automatically compressed.

---

## 7. Suggested Reading

- [XNet V2](api-xnet-v2.en.md)
- [Network TLS](api-network-tls.en.md)
- [XHTTP](api-xhttp.en.md)
- [Coroutine](api-coroutine.en.md)
- [Future / Task / Promise](api-future-task-promise.en.md)
- [Async HTTPD Handling Guide](../guide/httpd-async-intro.en.md)
