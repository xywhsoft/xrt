# XHTTP HTTP/1.1 Client Mainline

> The current HTTP/1.1 client mainline built on `xnet-v2 + xtlssession`.

[Back to Index](README.en.md)

---

## 1. Positioning

`xhttp` is the current mainline HTTP client. It provides:

- request and response objects
- synchronous and asynchronous execution
- one execution model built on `xnetengine`
- one path for both `http` and `https`
- one path for `SOCKS5 CONNECT / HTTP CONNECT` proxy access
- origin-based keep-alive reuse

It is designed to stay lightweight, infrastructure-friendly, and easy to connect with the unified async mainline.

---

## 2. Core Objects

### `xhttprequest`

The request object carries:

- method
- URL
- parsed URL fields
- headers
- body
- timeout
- peer-verification policy
- shared proxy object

### `xhttpresponse`

The response object carries:

- status code
- version / reason
- headers
- body
- body length

The current mainline primarily targets complete-request / complete-response flows.

---

## 3. Official API

```c
XXAPI void xrtHttpRequestInit(xhttprequest* pReq);
XXAPI void xrtHttpRequestUnit(xhttprequest* pReq);
XXAPI bool xrtHttpRequestSetMethod(xhttprequest* pReq, const char* sMethod);
XXAPI bool xrtHttpRequestSetURL(xhttprequest* pReq, const char* sURL);
XXAPI bool xrtHttpRequestSetHeader(xhttprequest* pReq, const char* sName, const char* sValue);
XXAPI bool xrtHttpRequestSetBodyCopy(xhttprequest* pReq, const void* pData, size_t iLen, const char* sContentType);
XXAPI void xrtHttpRequestSetTimeout(xhttprequest* pReq, uint32 iTimeoutMs);
XXAPI void xrtHttpRequestSetVerifyPeer(xhttprequest* pReq, bool bVerifyPeer);

XXAPI void xrtHttpResponseDestroy(xhttpresponse* pResp);
XXAPI const char* xrtHttpResponseHeader(const xhttpresponse* pResp, const char* sName);

XXAPI xnetfuture* xrtHttpExecuteAsync(xnetengine* pEngine, const xhttprequest* pReq);
XXAPI xhttpresponse* xrtHttpExecuteSync(xnetengine* pEngine, const xhttprequest* pReq, xnet_result* pStatus);
XXAPI void xrtHttpCloseIdleConnections(xnetengine* pEngine);
```

Notes:

- there is no extra `SetProxy` helper on the request object
- assign a shared proxy directly through `xhttprequest.pProxy`

---

## 4. Current Mainline Behavior

The current `xhttp` mainline:

- is built on `xnet_stream`
- supports plain HTTP and built-in TLS
- supports `SOCKS5 CONNECT` and `HTTP CONNECT`
- reuses keep-alive connections serially by origin
- supports chunked transfer
- is best suited today for whole-body request / whole-body response flows

Proxy-related behavior:

- pre-open sequencing is `TCP -> proxy handshake -> TLS -> HTTP request`
- keep-alive matching includes the proxy dimension, so identical origins behind different proxies are not mixed
- when `pProxy == NULL`, the request stays direct

This makes it a good fit for:

- REST / JSON APIs
- service-to-service HTTP
- Internet API calls
- non-browser LLM API clients

---

## 5. Relationship to Other Modules

- `xurl` handles URL parsing and host/header construction
- `xhttp_util` handles lower-level HTTP text helpers
- `xtlssession` handles the HTTPS path
- when a proxy is configured, TLS is established after the proxy tunnel is ready
- `ExecuteAsync` returns a future and can plug directly into coroutine and task orchestration

---

## 6. Suggested Reading

- [XNet V2](api-xnet-v2.en.md)
- [Network TLS](api-network-tls.en.md)
- [Future / Task / Promise](api-future-task-promise.en.md)
