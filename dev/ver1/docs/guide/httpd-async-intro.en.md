# Async HTTPD Handling

> A practical guide for using `xhttpd` asynchronous handlers with futures, delayed responses, and coroutine-produced futures.

[Back to Guide Index](README.en.md)

---

## 1. What Changed

`xhttpd` is no longer limited to the old synchronous `OnRequest(...) -> fill response -> return` model.

You can now:

- return a future from `OnRequestAsync`
- build the final response in another task
- respond later through `xrtHttpdConnRespond()`
- keep using the existing synchronous `OnRequest` path as a fallback

This is the recommended shape for:

- AI agent tool gateways
- slow upstream fan-out
- task / queue driven handlers
- coroutine-oriented service code

---

## 2. Event Model

```c
typedef struct {
	void (*OnOpen)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn);
	bool (*OnRequest)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, xhttpdresponse* pResp);
	xfuture* (*OnRequestAsync)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq);
	void (*OnClose)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, xnet_result iReason);
	void (*OnError)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, int iSysErr);
} xhttpdevents;
```

Dispatch rules:

1. `OnRequestAsync` gets the first chance.
2. If it returns `NULL`, `xhttpd` falls back to `OnRequest`.
3. If neither path handles the request, `xhttpd` sends `404 Not Found`.

---

## 3. Pattern A: Future Returns the Response

This is the simplest async style.

```c
static int32 procBuildResponse(ptr pArg, xfuture_result* pOut)
{
	xhttpdresponse* pResp = xrtHttpdResponseCreate();
	(void)pArg;
	if ( !pResp || !pOut ) return XRT_NET_ERROR;
	xrtHttpdResponseSetStatus(pResp, 200u, NULL);
	xrtHttpdResponseSetBodyCopy(pResp, "ready", 5u, "text/plain");
	pOut->pValue = pResp;
	return XRT_NET_OK;
}

static xfuture* procOnRequestAsync(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq)
{
	(void)pOwner;
	(void)pServer;
	(void)pConn;
	(void)pReq;
	return xTaskRunThread(procBuildResponse, NULL, 0);
}
```

Rules:

- allocate the response on the heap
- return it through `pOut->pValue`
- do not destroy it yourself after returning success

`xhttpd` takes ownership of both the returned future and the final response object.

---

## 4. Pattern B: Respond Later Manually

Use this when the handler needs to stream work through another subsystem before deciding when to reply.

```c
static int32 procRespondLater(ptr pArg, xfuture_result* pOut)
{
	xhttpdconn* pConn = (xhttpdconn*)pArg;
	xhttpdresponse* pResp = xrtHttpdResponseCreate();
	(void)pOut;
	if ( !pConn || !pResp ) return XRT_NET_ERROR;
	xrtHttpdResponseSetStatus(pResp, 202u, NULL);
	xrtHttpdResponseSetBodyCopy(pResp, "queued", 6u, "text/plain");
	if ( xrtHttpdConnRespond(pConn, pResp) != XRT_NET_OK ) {
		xrtHttpdResponseDestroy(pResp);
		return XRT_NET_ERROR;
	}
	xrtHttpdResponseDestroy(pResp);
	return XRT_NET_OK;
}

static xfuture* procOnRequestAsync(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq)
{
	(void)pOwner;
	(void)pServer;
	(void)pReq;
	return xTaskRunThread(procRespondLater, pConn, 0);
}
```

Important detail:

- if you already called `xrtHttpdConnRespond()`, the future should complete with `NULL`
- that tells `xhttpd` the response has already been committed

---

## 5. Pattern C: Coroutine-Produced Future

If your service code already lives in coroutine land, return a future produced by `xTaskRunCo()`.

```c
static int32 procBuildResponseCo(ptr pArg, xfuture_result* pOut)
{
	xhttpdresponse* pResp = xrtHttpdResponseCreate();
	(void)pArg;
	if ( !pResp || !pOut ) return XRT_NET_ERROR;
	xrtHttpdResponseSetStatus(pResp, 206u, NULL);
	xrtHttpdResponseSetBodyCopy(pResp, "co", 2u, "text/plain");
	pOut->pValue = pResp;
	return XRT_NET_OK;
}

static xfuture* procOnRequestAsync(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq)
{
	xcosched* pSched;
	xfuture* pFuture;
	(void)pOwner;
	(void)pServer;
	(void)pConn;
	(void)pReq;
	pSched = xrtCoSchedCreate();
	if ( !pSched ) return NULL;
	pFuture = xTaskRunCo(pSched, procBuildResponseCo, NULL, 0);
	if ( pFuture ) xrtCoSchedRun(pSched);
	xrtCoSchedDestroy(pSched);
	return pFuture;
}
```

The important part is not whether the future is pending or already completed. `xhttpd` accepts both.

---

## 6. Fallback and Error Mapping

`OnRequestAsync` can also choose not to handle a request:

- return `NULL` to fall back to `OnRequest`

If the future fails:

- timeout-like failures map to `408`
- cancelled / closed-like failures map to `503`
- generic failures map to `500`

This makes it easy to plug task-based code into HTTP without manually building every error response.

---

## 7. Keep-Alive Notes

`xhttpd` still keeps one request in flight per connection.

That means:

- async request A on a keep-alive connection blocks request B until A is really finished
- if you reply manually through `xrtHttpdConnRespond()`, `xhttpd` still tracks the request until the async future completes
- this prevents use-after-free and request interleaving bugs on the same connection

---

## 8. Recommended Reading

- [XHTTPD API](../api/api-xhttpd.en.md)
- [Coroutine](../api/api-coroutine.en.md)
- [Future / Task / Promise](../api/api-future-task-promise.en.md)
