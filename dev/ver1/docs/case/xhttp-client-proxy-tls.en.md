# XRT Case Study: XHTTP Client Chain with URL, Proxy, and TLS

> A stable client-side mainline: parse the URL first, attach an optional shared proxy, let HTTPS stay on the TLS path, and then choose sync or async waiting according to the real lifecycle.

[中文](xhttp-client-proxy-tls.md) | [Back to Case Studies](README.en.md)

---

## 1. Scenario

Assume you are building a client program that calls an external HTTPS API with these constraints:

1. you need to know what the URL will actually connect to
2. some environments go direct, while others must use a proxy
3. most requests are HTTPS, not only plain HTTP
4. some calls are one-shot sync probes, while others must join an async flow
5. several requests against the same origin should reuse keep-alive connections when possible
6. the same code should stay valid on both Windows and Linux

Without one clear mainline, client code usually degrades into:

- hand-built URL parsing
- proxy settings scattered across call sites
- TLS treated as an afterthought
- completely separate sync and async call paths

This case explains how the current XRT public networking mainline should be used for a real HTTPS client.

---

## 2. Why This Mainline

### Why not use `xnet_stream` directly?

Because the main problem here is not:

- manually managing sockets
- manually assembling request lines and headers
- manually driving keep-alive and response parsing

If the real goal is "call an HTTP API", then the correct entry point is `xhttp`.

### Why is `xhttp` alone still not enough?

Because the real client boundary is wider than `ExecuteSync()` or `ExecuteAsync()`.

You still need to reason about:

- how the URL is split
- what the `Host` header becomes
- when `https` enters the TLS path
- how a shared proxy object should live
- how the hidden-engine sync path differs from an explicit-engine async path

That is why this page teaches:

- `xurl + xhttp + proxy + TLS + future`

as one continuous chain.

---

## 3. What Each Layer Does

| Layer | Role in this case | What it really gives you |
|------|-------------------|--------------------------|
| `xurl` | parse the URL, build `Host`, copy the target | see clearly where the request will really go |
| `xhttp_util` | low-level HTTP text helper layer behind `xhttp` | header/token/chunked details without handwritten parsing |
| `xnetproxy` | shared proxy object | one entry for direct, HTTP CONNECT, and SOCKS5 paths |
| `xtlssession` | TLS mainline under `https` | TLS stays inside the network chain |
| `xhttp` | formal HTTP client entry | request objects, response objects, sync and async execution |
| `xnetengine / xnetfuture` | explicit engine and async waiting | keep-alive reuse and future-based waiting |

One-sentence version:

> `xurl` explains the target, `xhttp` sends the request, `proxy/TLS` shape the middle chain, and `xnetfuture` lets the async result join the unified waiting model.

---

## 4. Code Skeleton

The reviewed Chinese page contains the full version. The skeleton below keeps the same mainline and the most important ownership boundaries:

```c
#include "xrt.h"
#include <stdio.h>
#include <string.h>

static void procExplainURL(const char* sURL)
{
	xrturlview tURL;
	char sHostHeader[320];
	char sTarget[2048];

	if ( (sURL == NULL) || !xrtUrlParseView(sURL, &tURL) ) {
		return;
	}

	(void)xrtUrlMakeHostHeader(&tURL, sHostHeader, sizeof(sHostHeader));
	(void)xrtUrlViewCopyTargetTo(&tURL, sTarget, sizeof(sTarget));
	printf("host-header=%s target=%s\n", sHostHeader, sTarget);
}

static bool procInitRequest(xhttprequest* pReq, const char* sMethod, const char* sURL, xnetproxy* pProxy)
{
	xrtHttpRequestInit(pReq);

	if ( !xrtHttpRequestSetMethod(pReq, sMethod) ) {
		return FALSE;
	}
	if ( !xrtHttpRequestSetURL(pReq, sURL) ) {
		return FALSE;
	}

	(void)xrtHttpRequestSetHeader(pReq, "Accept", "application/json");
	xrtHttpRequestSetTimeout(pReq, 10000u);
	xrtHttpRequestSetIdleTimeout(pReq, 3000u);
	xrtHttpRequestSetVerifyPeer(pReq, TRUE);
	pReq->pProxy = (pProxy != NULL) ? xrtNetProxyAddRef(pProxy) : NULL;
	return TRUE;
}

static bool procRunSyncProbe(const char* sURL, xnetproxy* pProxy)
{
	xhttprequest tReq;
	xhttpresponse* pResp = NULL;
	xnet_result iStatus = XRT_NET_ERROR;
	xnetengine* pHidden = NULL;
	bool bOk = FALSE;

	memset(&tReq, 0, sizeof(tReq));

	if ( !procInitRequest(&tReq, "GET", sURL, pProxy) ) {
		goto cleanup;
	}

	pResp = xrtHttpExecuteSync(NULL, &tReq, &iStatus);
	if ( (iStatus == XRT_NET_OK) && (pResp != NULL) ) {
		printf("sync: code=%u len=%u\n", (unsigned)pResp->iStatusCode, (unsigned)pResp->iBodyLen);
		bOk = TRUE;
	}

cleanup:
	if ( pResp != NULL ) {
		xrtHttpResponseDestroy(pResp);
	}
	xrtHttpRequestUnit(&tReq);

	pHidden = xrtNetSyncGetHiddenEngine();
	if ( pHidden != NULL ) {
		xrtHttpCloseIdleConnections(pHidden);
	}
	xrtNetSyncShutdownHiddenEngine();
	return bOk;
}

static bool procRunAsyncPair(const char* sURLA, const char* sURLB, xnetproxy* pProxy)
{
	xnetengineconfig tEngineCfg;
	xnetengine* pEngine = NULL;
	xhttprequest tReqA;
	xhttprequest tReqB;
	xnetfuture* pFutureA = NULL;
	xnetfuture* pFutureB = NULL;
	xhttpresponse* pRespA = NULL;
	xhttpresponse* pRespB = NULL;
	bool bOk = FALSE;

	memset(&tReqA, 0, sizeof(tReqA));
	memset(&tReqB, 0, sizeof(tReqB));
	xrtNetEngineConfigInit(&tEngineCfg);
	tEngineCfg.iWorkerCount = 1u;

	pEngine = xrtNetEngineCreate(&tEngineCfg);
	if ( (pEngine == NULL) || (xrtNetEngineStart(pEngine) != XRT_NET_OK) ) {
		goto cleanup;
	}

	if ( !procInitRequest(&tReqA, "GET", sURLA, pProxy) || !procInitRequest(&tReqB, "GET", sURLB, pProxy) ) {
		goto cleanup;
	}

	pFutureA = xrtHttpExecuteAsync(pEngine, &tReqA);
	if ( (pFutureA == NULL) || (xrtNetFutureWait(pFutureA, 10000u) != XRT_NET_OK) ) {
		goto cleanup;
	}
	pRespA = (xhttpresponse*)xrtNetFutureValue(pFutureA);
	printf("async A: code=%u\n", (unsigned)pRespA->iStatusCode);

	pFutureB = xrtHttpExecuteAsync(pEngine, &tReqB);
	if ( (pFutureB == NULL) || (xrtNetFutureWait(pFutureB, 10000u) != XRT_NET_OK) ) {
		goto cleanup;
	}
	pRespB = (xhttpresponse*)xrtNetFutureValue(pFutureB);
	printf("async B: code=%u\n", (unsigned)pRespB->iStatusCode);
	bOk = TRUE;

cleanup:
	if ( pRespA != NULL ) {
		xrtHttpResponseDestroy(pRespA);
	}
	if ( pRespB != NULL ) {
		xrtHttpResponseDestroy(pRespB);
	}
	if ( pFutureA != NULL ) {
		xrtNetFutureDestroy(pFutureA);
	}
	if ( pFutureB != NULL ) {
		xrtNetFutureDestroy(pFutureB);
	}
	xrtHttpRequestUnit(&tReqA);
	xrtHttpRequestUnit(&tReqB);

	if ( pEngine != NULL ) {
		xrtHttpCloseIdleConnections(pEngine);
		xrtNetEngineStop(pEngine);
		xrtNetEngineDestroy(pEngine);
	}
	return bOk;
}
```

---

## 5. How to Read the Skeleton

### 5.1 Start with `xurl`

The sample does not send a request immediately. It first uses:

- `xrtUrlParseView()`
- `xrtUrlMakeHostHeader()`
- `xrtUrlViewCopyTargetTo()`

That lets you inspect:

- whether the scheme is really `https`
- whether the `Host` header includes a port
- what the actual request target looks like

This looks optional at first, but it becomes valuable in proxy, redirect, and request-signing scenarios.

### 5.2 Proxy ownership is formal

The proxy is not a loose per-socket setting. It is a shared object:

- `xnetproxy`

Each request then takes its own reference through:

- `xhttprequest.pProxy`

So the lifecycle is:

- create the proxy once
- `AddRef` when a request needs it
- let `RequestUnit()` release the request-owned reference
- release the outer owner reference at the end

### 5.3 Why `https` does not require manual TLS driving

Because this chain already stands on:

- `xhttp`
- `xnet-v2`
- `xtlssession`

So when the URL is `https://...`, TLS is already part of the formal network path.

### 5.4 Hidden-engine sync path vs explicit-engine async path

The synchronous probe uses:

- `xrtHttpExecuteSync(NULL, &tReq, &iStatus)`

That means:

- let XRT use the hidden-engine path

This is a good fit for:

- one-shot sync calls
- small CLI tools
- startup probes

The async pair uses:

- explicit `xrtNetEngineCreate()`
- explicit `xrtHttpExecuteAsync()`

This is a better fit for:

- keep-alive reuse
- multi-request orchestration
- later integration with `future / wait-source / coroutine`

### 5.5 Why the async pair is sequential

The goal here is not to show off concurrency first.

The goal is to keep the mainline easy to read:

- same origin
- same proxy object
- same client engine

That keeps the keep-alive reuse discussion easy to understand.

---

## 6. Common Mistakes

### Mistake 1: treating the URL as just a string

That usually becomes painful once ports, targets, or proxy debugging start to matter.

### Mistake 2: attaching a bare proxy pointer without respecting reference ownership

Requests should hold their own proxy reference, not "borrow" an outer pointer informally.

### Mistake 3: forgetting to configure `VerifyPeer` when testing against a local self-signed service

That mixes certificate failures with business-logic failures.

### Mistake 4: using the hidden-engine sync path and then forgetting cleanup

Short demos may survive that. Long-lived tools will not stay clean.

### Mistake 5: forgetting to destroy both the response object and the future object

`xhttpresponse` and `xnetfuture` are formal objects, not temporary borrowed values.

---

## 7. Suggested Reading

- [Guide: xnet-v2 and TLS Session Intro](../guide/xnet-v2-tls-intro.en.md)
- [Guide: Proxy Objects and Connection Chains](../guide/proxy-intro.en.md)
- [XHTTP API](../api/api-xhttp.en.md)
- [XURL API](../api/api-xurl.en.md)
- [Network TLS API](../api/api-network-tls.en.md)
- [Case: Subprocess + Async File Tooling Pipeline](subprocess-file-async-pipeline.en.md)
- [Case: Streaming LLM API with XRT](streaming-llm-api.en.md)

---

**One-sentence summary:** the key is not merely "send one GET request"; the key is to explain the URL first, keep proxy and TLS inside the formal network chain, and then choose hidden-engine sync or explicit-engine async according to the real client lifecycle.
