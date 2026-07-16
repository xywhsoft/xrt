# XHTTP HTTP/1.1 Client

`xhttp` is XRT's HTTP/1.1 client on top of `xnet-v2` and `xtlssession`. HTTP/2 is intentionally out of scope.

[Back to Index](README.en.md)

## Capabilities

- plain HTTP and built-in TLS
- direct, SOCKS5, and HTTP CONNECT transport
- copied or streaming request bodies
- buffered or incrementally delivered response bodies
- streaming gzip and deflate response decompression
- content-length, chunked, and close-delimited responses
- total and idle timeouts
- explicit cancellation
- dynamically growing request and response header collections
- dynamically stored methods, URLs, hosts, targets, reason phrases, and trailers
- strict HTTP/1.1 framing and configurable parser limits
- default compatibility pool or client-scoped keep-alive pools

## Request and response ownership

Initialize every stack request with `xrtHttpRequestInit()` and release it with `xrtHttpRequestUnit()`. The request owns copied body bytes, expanded header storage, the complete name and value of every header, and its proxy reference.

A successful future owns no response automatically. The caller receives the `xhttpresponse*` value and must call `xrtHttpResponseDestroy()`.

For streaming execution, the final response contains status and header metadata. `pBody` remains `NULL`, while `iBodyLen` records the number of decoded bytes delivered to `OnBody`.

Do not copy an initialized request by value, with `memcpy`, or by mutating header entries directly. Reinitialize another request and populate it through the public setters instead.

## Redirect contract

`xrtHttpClientConfigInit()` enables redirects by default with a limit of 10 followed hops. The policy is controlled by `iFlags` and `iMaxRedirects`:

- `XHTTP_CLIENT_F_FOLLOW_REDIRECTS` follows 301, 302, 303, 307, and 308 responses that contain exactly one `Location` field.
- `XHTTP_CLIENT_F_REDIRECT_POST_TO_GET` applies the common 301/302 POST-to-GET behavior. A 303 always becomes GET, except that HEAD remains HEAD. Rewritten requests drop body framing and content metadata.
- 307 and 308 preserve the method and body. A streaming body must carry `XHTTP_BODY_F_REPLAYABLE`; each hop opens a new reader.
- Cross-origin hops remove explicit `Cookie` and, unless `XHTTP_CLIENT_F_ALLOW_CROSS_ORIGIN_AUTH` is set, remove `Authorization` and `Proxy-Authorization`. A configured CookieJar is evaluated again for the destination URL.
- HTTPS-to-HTTP redirects fail with `XHTTP_ERROR_REDIRECT_DOWNGRADE` unless `XHTTP_CLIENT_F_ALLOW_HTTPS_DOWNGRADE` is set.

The total deadline and cancellation context cover the complete redirect chain. Intermediate response bodies are drained for connection reuse but are not delivered to streaming callbacks. `xrtHttpResponseURL()` returns the final effective URL, and `xhttpdiagnostics.iRedirectCount` reports the number of followed hops.

## Response decompression contract

`xrtHttpClientConfigInit()` enables `XHTTP_CLIENT_F_AUTO_DECOMPRESS` by default. When the caller has not supplied `Accept-Encoding`, the client sends `Accept-Encoding: gzip, deflate`; an explicit request header is preserved unchanged.

For a final, body-bearing response, the client decodes `gzip`, legacy `x-gzip`, and `deflate` content codings. Stacked codings are decoded in reverse application order, up to four non-identity layers. `deflate` accepts both the zlib-wrapped form required by HTTP and the raw form emitted by older servers. Unknown, malformed, or excessive coding lists remain raw with their headers intact. Intermediate redirect bodies and bodyless responses are never decoded.

After successful activation, buffered bodies and streaming `OnBody` callbacks receive decoded bytes. The response has `XHTTP_RESP_F_DECOMPRESSED`, its public `Content-Encoding` and encoded `Content-Length` fields are removed, and its final content length is the decoded byte count. `xrtHttpResponseOriginalContentEncoding()` returns the exact combined coding value that was removed. The returned pointer is borrowed and remains valid until `xrtHttpResponseDestroy()`.

Both encoded and decoded body sizes are independently bounded by `tHttp1Limits.iMaxBodyBytes`. A malformed, truncated, checksum-invalid, or over-expanded stream fails with `XHTTP_ERROR_DECOMPRESSION`; partial decoded data is never reported as a successful response. Defining `XRT_NO_XINFLATE` removes the inflater, disables the default flag, and makes `xrtHttpClientCreateEx()` reject an explicitly enabled decompression flag.

## Header API

```c
bool xrtHttpRequestSetHeader(xhttprequest* req, const char* name, const char* value);
bool xrtHttpRequestAddHeader(xhttprequest* req, const char* name, const char* value);
uint32 xrtHttpRequestRemoveHeader(xhttprequest* req, const char* name);
bool xrtHttpRequestSetAuthorization(xhttprequest* req, const char* scheme, const char* credentials);
bool xrtHttpRequestSetBasicAuth(xhttprequest* req, const char* user, const char* password);
bool xrtHttpRequestSetBearerAuth(xhttprequest* req, const char* token);
void xrtHttpRequestClearAuthorization(xhttprequest* req);

const char* xrtHttpRequestMethod(const xhttprequest* req);
const char* xrtHttpRequestURL(const xhttprequest* req);
const char* xrtHttpRequestHost(const xhttprequest* req);
const char* xrtHttpRequestTarget(const xhttprequest* req);

const char* xrtHttpResponseHeader(const xhttpresponse* resp, const char* name);
uint32 xrtHttpResponseHeaderCount(const xhttpresponse* resp);
const char* xrtHttpResponseHeaderNameAt(const xhttpresponse* resp, uint32 index);
const char* xrtHttpResponseHeaderValueAt(const xhttpresponse* resp, uint32 index);
const char* xrtHttpResponseTrailer(const xhttpresponse* resp, const char* name);
uint32 xrtHttpResponseTrailerCount(const xhttpresponse* resp);
const char* xrtHttpResponseTrailerNameAt(const xhttpresponse* resp, uint32 index);
const char* xrtHttpResponseTrailerValueAt(const xhttpresponse* resp, uint32 index);
const char* xrtHttpResponseURL(const xhttpresponse* resp);
const char* xrtHttpResponseOriginalContentEncoding(const xhttpresponse* resp);
```

`SetHeader` replaces the first case-insensitive match. `AddHeader` preserves duplicates such as `Set-Cookie`. `RemoveHeader` removes every case-insensitive match.

The authorization helpers replace the single `Authorization` field. The generic helper validates the auth scheme as an HTTP token and rejects invalid field values. Basic auth encodes the exact `user:password` bytes and rejects a colon in the user name; Bearer treats the token as opaque field data. Redirects retain authorization only for same-origin hops unless the explicit cross-origin authorization flag is enabled.

Header names and values are dynamically stored at their exact lengths. Methods, URLs, parsed hosts and request targets, response reason phrases, and trailers follow the same rule. They are not constrained or silently truncated by the legacy per-field arrays represented by `XHTTP_*_CAP`. Request strings live until `xrtHttpRequestUnit()` and response strings live until `xrtHttpResponseDestroy()`. The old capacity macros remain for source compatibility only and are not runtime limits.

## HTTP/1.1 framing contract

Incoming messages reject ambiguous or invalid framing: `Transfer-Encoding` together with `Content-Length`, conflicting repeated `Content-Length` values, obsolete folded headers, invalid field names or values, unsupported transfer codings, malformed chunks, and forbidden trailer fields. Repeated or comma-separated `Content-Length` is accepted only when every parsed value is identical.

Outgoing requests enforce the same metadata rules before network I/O. A supplied `Content-Length` must exactly match the buffered body, `Transfer-Encoding` cannot coexist with it, and the client only emits the supported final `chunked` coding. Invalid request metadata is reported as `XHTTP_ERROR_INVALID_ARGUMENT`; allocation failures remain `XHTTP_ERROR_OUT_OF_MEMORY`.

Initialize `xhttpclientconfig` with `xrtHttpClientConfigInit()` before overriding `tHttp1Limits` or `iMaxInformationalResponses`, then pass it to `xrtHttpClientCreateEx()`. The limits bound start-line size, total header bytes, header-line length, header count, body bytes, chunk-line length, and trailer count. Informational responses are consumed before the final response and are independently bounded to prevent unbounded `1xx` loops.

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

Chunk framing is removed before `OnBody` runs. Parsed trailers are attached to the final response and are available through the trailer accessors after execution completes.

## Cancellation

```c
if ( !xrtHttpCancel(future) ) {
	/* the request may already be complete */
}
```

Cancellation resolves the future with `XRT_NET_CANCELLED` and aborts the active connection. It works for both buffered and streaming execution.

## Per-request diagnostics

Attach a caller-owned `xhttpdiagnostics` before execution when the provider layer needs stable failure classification and timing data:

```c
xhttpdiagnostics diagnostics;

xrtHttpRequestSetDiagnostics(&req, &diagnostics);
future = xrtHttpExecuteAsync(engine, &req);
status = xrtNetFutureWait(future, XNET_WAIT_INFINITE);

printf("error=%s phase=%s total=%llu ms\n",
	xrtHttpErrorCodeName(diagnostics.eError),
	xrtHttpPhaseName(diagnostics.ePhase),
	(unsigned long long)diagnostics.iTotalDurationMs);
```

The diagnostics object is borrowed and must outlive the asynchronous request. It is initialized when execution starts and contains the final transport status, system error, total/idle timeout distinction, cancellation, protocol/callback/decompression failures, connection reuse, monotonic milestones, derived durations, and byte counts after the future completes. `iResponseBodyBytes` is the decoded/delivered count for the final response, while `iResponseWireBodyBytes` is its encoded HTTP body count. Successful responses also own the same final snapshot, available through `xrtHttpResponseDiagnostics()`.

XRT reports transport facts but does not retry requests automatically. Retry and idempotency policy belongs to the caller (for example, an LLM provider adapter).

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

`xrtHttpClientConfigInit()` configures a bounded pool by default: 128 idle connections in total, 8 per origin, and a 90-second idle timeout. `iMaxIdleConnections` is the client-wide bound, `iMaxIdleConnectionsPerOrigin` is the bound for one transport origin (scheme, host, port, proxy, TLS verification policy, and engine), and `iIdleConnectionTimeoutMs` controls idle expiry. Setting either capacity to zero disables connection reuse. Setting only the timeout to zero keeps bounded idle connections until reuse, eviction, explicit close, peer close, or client destruction. When a bound is reached, the oldest applicable idle connection is evicted.

Pool and lifetime counters are available without taking ownership of internal objects:

```c
xhttpclientstats stats;
xrtHttpClientStatsInit(&stats);
if (xrtHttpClientGetStats(client, &stats)) {
	printf("active=%u idle=%u opened=%llu reused=%llu\n",
		stats.iActiveConnections, stats.iIdleConnections,
		(unsigned long long)stats.iConnectionsOpened,
		(unsigned long long)stats.iConnectionsReused);
}
```

The current connection counts are a concurrent snapshot. Lifetime counters are monotonic for the client: started/completed requests, successful connection opens, pool reuses, closes, followed redirects, request wire bytes, and encoded response body bytes across all redirect hops. Connection close cleanup is asynchronous, so `iConnectionsClosed` can advance shortly after a request future completes.

Compatibility functions (`xrtHttpExecuteAsync`, `xrtHttpExecuteSync`, and `xrtHttpCloseIdleConnections`) continue to use the process-level default pool.

## Current boundary

- request bodies may be copied or streamed from a replayable/non-replayable `xhttpbody` factory
- one HTTP/1.1 request is active per connection; no pipelining
- HTTP/2 is not planned for this phase

## Related modules

- [XNet V2](api-xnet-v2.en.md)
- [Network TLS](api-network-tls.en.md)
- [Future / Task / Promise](api-future-task-promise.en.md)
