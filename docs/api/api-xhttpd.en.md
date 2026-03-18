# XHTTPD HTTP/1.1 Server Mainline

> The current HTTP/1.1 server mainline built on `xnet-v2 + xnet_stream + xtlssession`.

[Back to Index](README.en.md)

---

## 1. Positioning

`xhttpd` is the current mainline HTTP server. It is responsible for:

- listening
- accepting connections
- parsing HTTP/1.1 requests
- materializing request / response objects
- supporting both plain HTTP and TLS service paths
- cooperating with `xnetengine`, coroutines, and the async mainline

It is meant to be light enough for infrastructure code while still directly usable for service development.

---

## 2. Core Types

Important types include:

- `xhttpdconfig`
- `xhttpdrequest`
- `xhttpdresponse`
- `xhttpdevents`

The most important callback is:

- `OnRequest`

which reads the request and fills the response.

---

## 3. Official API

```c
XXAPI void xrtHttpdConfigInit(xhttpdconfig* pCfg);
XXAPI void xrtHttpdRequestInit(xhttpdrequest* pReq);
XXAPI void xrtHttpdRequestUnit(xhttpdrequest* pReq);
XXAPI void xrtHttpdResponseInit(xhttpdresponse* pResp);
XXAPI void xrtHttpdResponseUnit(xhttpdresponse* pResp);
XXAPI void xrtHttpdResponseSetStatus(xhttpdresponse* pResp, uint32 iStatusCode, const char* sReason);
XXAPI bool xrtHttpdResponseSetHeader(xhttpdresponse* pResp, const char* sName, const char* sValue);
XXAPI bool xrtHttpdResponseSetBodyCopy(xhttpdresponse* pResp, const void* pData, size_t iLen, const char* sContentType);
XXAPI const char* xrtHttpdRequestHeader(const xhttpdrequest* pReq, const char* sName);

XXAPI xhttpdserver* xrtHttpdCreate(xnetengine* pEngine, const xhttpdconfig* pCfg, const xhttpdevents* pEvents, ptr pUserData);
XXAPI uint16 xrtHttpdBoundPort(const xhttpdserver* pServer);
XXAPI xnet_result xrtHttpdStart(xhttpdserver* pServer);
XXAPI void xrtHttpdStop(xhttpdserver* pServer);
XXAPI void xrtHttpdDestroy(xhttpdserver* pServer);
```

---

## 4. Recommended Mainline Use

The current recommended way to use `xhttpd` is:

- as the formal entry for a minimal HTTP service
- as the formal entry for JSON APIs
- as the upper-layer carrier for template rendering and AI result presentation

That is why the current docs recommend:

- `runtime + xhttpd + xvalue + json`

instead of rebuilding service logic directly from low-level sockets and raw HTTP parsing.

---

## 5. Suggested Reading

- [XNet V2](api-xnet-v2.en.md)
- [Value](api-value.en.md)
- [JSON](api-json.en.md)
- [Template](api-template.en.md)
