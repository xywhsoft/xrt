# XHTTP HTTP/1.1 Client

`xhttp` is XRT's HTTP/1.1 client on top of `xnet-v2` and `xtlssession`. HTTP/2 is intentionally out of scope.

[Back to Index](README.en.md)

## Capabilities

- plain HTTP and built-in TLS
- direct, SOCKS5, and HTTP CONNECT transport
- buffered request bodies
- buffered or incrementally delivered response bodies
- content-length, chunked, and close-delimited responses
- total and idle timeouts
- explicit cancellation
- dynamically growing request and response header collections
- default compatibility pool or client-scoped keep-alive pools

## Request and response ownership

Initialize every stack request with `xrtHttpRequestInit()` and release it with `xrtHttpRequestUnit()`. The request owns copied body bytes, expanded header storage, and its proxy reference.

A successful future owns no response automatically. The caller receives the `xhttpresponse*` value and must call `xrtHttpResponseDestroy()`.

For streaming execution, the final response contains status and header metadata. `pBody` remains `NULL`, while `iBodyLen` records the number of decoded bytes delivered to `OnBody`.

## Header API

```c
bool xrtHttpRequestSetHeader(xhttprequest* req, const char* name, const char* value);
bool xrtHttpRequestAddHeader(xhttprequest* req, const char* name, const char* value);
uint32 xrtHttpRequestRemoveHeader(xhttprequest* req, const char* name);

const char* xrtHttpResponseHeader(const xhttpresponse* resp, const char* name);
uint32 xrtHttpResponseHeaderCount(const xhttpresponse* resp);
const char* xrtHttpResponseHeaderNameAt(const xhttpresponse* resp, uint32 index);
const char* xrtHttpResponseHeaderValueAt(const xhttpresponse* resp, uint32 index);
```

`SetHeader` replaces the first case-insensitive match. `AddHeader` preserves duplicates such as `Set-Cookie`. `RemoveHeader` removes every case-insensitive match.

## Buffered execution

```c
xhttprequest req;
xhttpresponse* resp;
xnet_result status;

xrtHttpRequestInit(&req);
xrtHttpRequestSetMethod(&req, "POST");
xrtHttpRequestSetURL(&req, "https://example.com/v1/chat");
xrtHttpRequestSetHeader(&req, "Authorization", "Bearer ...");
xrtHttpRequestSetBodyCopy(&req, json, strlen(json), "application/json");
xrtHttpRequestSetTimeout(&req, 120000u);
xrtHttpRequestSetIdleTimeout(&req, 30000u);

resp = xrtHttpExecuteSync(engine, &req, &status);
if ( resp ) {
	/* consume resp->pBody */
	xrtHttpResponseDestroy(resp);
}
xrtHttpRequestUnit(&req);
```

`xrtHttpExecuteAsync()` uses the same transport path and returns an `xnetfuture*`.

## Streaming execution

Callbacks run on the connection's network worker. The data pointer passed to `OnBody` is valid only for the duration of that callback. Returning `false` from either callback cancels the request.

```c
static bool on_headers(ptr user, const xhttpresponse* resp)
{
	return resp->iStatusCode >= 200u;
}

static bool on_body(ptr user, const void* data, size_t len)
{
	return consume_sse(user, data, len);
}

xhttpstreamcallbacks callbacks;
xnetfuture* future;

xrtHttpStreamCallbacksInit(&callbacks);
callbacks.pUserData = state;
callbacks.OnHeaders = on_headers;
callbacks.OnBody = on_body;

future = xrtHttpExecuteStreamAsync(engine, &req, &callbacks);
status = xrtNetFutureWait(future, XNET_WAIT_INFINITE);
resp = status == XRT_NET_OK ? xrtNetFutureValue(future) : NULL;
```

Chunk framing is removed before `OnBody` runs. Trailers are consumed but are not yet exposed as response metadata.

## Cancellation

```c
if ( !xrtHttpCancel(future) ) {
	/* the request may already be complete */
}
```

Cancellation resolves the future with `XRT_NET_CANCELLED` and aborts the active connection. It works for both buffered and streaming execution.

## Client-scoped connection pools

Use one `xhttpclient` for a long-lived provider or service boundary. It binds to one `xnetengine` and owns a separate keep-alive pool.

```c
xhttpclient* client = xrtHttpClientCreate(engine);

future = xrtHttpClientExecuteStreamAsync(client, &req, &callbacks);
/* wait for and release future/response */

xrtHttpClientCloseIdle(client);
xrtHttpClientDestroy(client);
```

The client may be destroyed while connections are finishing: internal references keep it alive until asynchronous close cleanup completes. Do not start new calls through a client after destroying it.

Compatibility functions (`xrtHttpExecuteAsync`, `xrtHttpExecuteSync`, and `xrtHttpCloseIdleConnections`) continue to use the process-level default pool.

## Current boundary

- request bodies are buffered before sending
- response trailers are not exposed
- one HTTP/1.1 request is active per connection; no pipelining
- HTTP/2 is not planned for this phase

## Related modules

- [XNet V2](api-xnet-v2.en.md)
- [Network TLS](api-network-tls.en.md)
- [Future / Task / Promise](api-future-task-promise.en.md)
