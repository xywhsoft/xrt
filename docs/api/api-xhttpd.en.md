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

- request / response bodies are still whole-message oriented
- static files, routing tables, and generic upgrade dispatch are still deferred
- PTY-style terminal interaction is unrelated to this layer

---

## 2. Core Types

### `xhttpdconfig`

Important fields:

- `tBindAddr`
- `iFlags`
- `iBacklog`
- `iRecvLimit`
- `pTlsConfig`

### `xhttpdrequest`

Materialized request object:

- request line fields: `sMethod`, `sTarget`, `sPath`, `sQuery`, `sVersion`
- header array: `arrHeaders`
- body pointer and size: `pBody / iBodyLen`
- flags such as `XHTTPD_REQ_F_KEEPALIVE`

### `xhttpdresponse`

Materialized response object:

- status line fields: `iStatusCode`, `sReason`
- header array: `arrHeaders`
- body pointer and size: `pBody / iBodyLen`
- close policy flags such as `XHTTPD_RESP_F_CLOSE`

### `xhttpdevents`

`xhttpd` now supports both synchronous and asynchronous request handling:

```c
typedef struct {
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
XXAPI void xrtHttpdRequestInit(xhttpdrequest* pReq);
XXAPI void xrtHttpdRequestUnit(xhttpdrequest* pReq);
XXAPI void xrtHttpdResponseInit(xhttpdresponse* pResp);
XXAPI void xrtHttpdResponseUnit(xhttpdresponse* pResp);
XXAPI xhttpdresponse* xrtHttpdResponseCreate(void);
XXAPI void xrtHttpdResponseDestroy(xhttpdresponse* pResp);
XXAPI void xrtHttpdResponseSetStatus(xhttpdresponse* pResp, uint32 iStatusCode, const char* sReason);
XXAPI bool xrtHttpdResponseSetHeader(xhttpdresponse* pResp, const char* sName, const char* sValue);
XXAPI bool xrtHttpdResponseSetBodyCopy(xhttpdresponse* pResp, const void* pData, size_t iLen, const char* sContentType);
XXAPI const char* xrtHttpdRequestHeader(const xhttpdrequest* pReq, const char* sName);
XXAPI const char* xrtHttpdResponseHeader(const xhttpdresponse* pResp, const char* sName);
```

### Async Connection Helpers

```c
XXAPI bool xrtHttpdConnIsOpen(const xhttpdconn* pConn);
XXAPI xnet_result xrtHttpdConnRespond(xhttpdconn* pConn, const xhttpdresponse* pResp);
XXAPI xnet_result xrtHttpdConnClose(xhttpdconn* pConn, uint32 iCloseFlags);
```

These helpers let asynchronous handlers send the response later from another task, thread, or coroutine-produced future.

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
- `xrtHttpdConnRespond()` still sends a whole response message, not a streaming body API.
- If you need websocket upgrade or protocol switching, keep using the dedicated `xws` mainline.
- If you need large file streaming, range support, or generic middleware chains, those should stay in upper layers for now.

---

## 7. Suggested Reading

- [XNet V2](api-xnet-v2.en.md)
- [Network TLS](api-network-tls.en.md)
- [XHTTP](api-xhttp.en.md)
- [Coroutine](api-coroutine.en.md)
- [Future / Task / Promise](api-future-task-promise.en.md)
- [Async HTTPD Handling Guide](../guide/httpd-async-intro.en.md)
