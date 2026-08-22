# XHTTPD 异步处理入门

> 一篇面向实际服务代码的入门说明：怎么在 `xhttpd` 里使用 future、延迟回包，以及 coroutine 产出的 future。

[返回教学索引](README.md)

---

## 1. 这次补了什么

`xhttpd` 不再只支持旧的同步模式：

- `OnRequest(...) -> 填充响应 -> return`

现在还可以：

- 在 `OnRequestAsync` 里返回 future
- 在别的 task / 线程里生成最终响应
- 通过 `xrtHttpdConnRespond()` 手动延迟回包
- 保留现有 `OnRequest` 作为 fallback

这很适合：

- AI Agent 工具网关
- 慢上游 fan-out
- 队列 / 任务驱动的 handler
- coroutine 风格的服务主线

---

## 2. 事件模型

```c
typedef struct {
	void (*OnOpen)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn);
	bool (*OnRequest)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, xhttpdresponse* pResp);
	xfuture* (*OnRequestAsync)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq);
	void (*OnClose)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, xnet_result iReason);
	void (*OnError)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, int iSysErr);
} xhttpdevents;
```

分发规则：

1. `OnRequestAsync` 先拿到机会。
2. 如果它返回 `NULL`，就回退到 `OnRequest`。
3. 两条路都没处理时，`xhttpd` 自动返回 `404 Not Found`。

---

## 3. 模式 A：future 直接产出响应对象

这是最简单的异步写法。

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

这条路径的规则是：

- 响应对象要放在堆上
- 通过 `pOut->pValue` 返回给 `xhttpd`
- 成功后不要自己再销毁

future 和最终的 `xhttpdresponse*` 所有权都会转交给 `xhttpd`。

---

## 4. 模式 B：手动延迟回包

如果 handler 需要先把工作交给别的子系统，再决定什么时候回复，可以走这条路。

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

这里有个关键约定：

- 如果你已经调用了 `xrtHttpdConnRespond()`，future 成功完成时就应该返回 `NULL`

这代表“响应已经手动提交过了”。

---

## 5. 模式 C：coroutine 产出的 future

如果你的服务代码本来就是 coroutine 风格，可以直接把 `xTaskRunCo()` 产出的 future 交给 `xhttpd`。

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

对 `xhttpd` 来说，future 是 pending 还是已经完成都可以接。

---

## 6. fallback 和错误映射

`OnRequestAsync` 也可以选择“不处理这个请求”：

- 返回 `NULL`，让 `xhttpd` 回退到 `OnRequest`

如果 future 失败：

- timeout 类失败会映射成 `408`
- cancelled / closed 类失败会映射成 `503`
- 普通失败会映射成 `500`

这样你把 task / future 代码接进 HTTP 层时，不需要每次都手写错误响应模板。

---

## 7. keep-alive 要点

`xhttpd` 仍然保持“一个连接同一时刻只有一个活动请求”。

这意味着：

- keep-alive 连接上的异步请求 A 没有真正结束前，请求 B 不会推进
- 即使你已经手动调用了 `xrtHttpdConnRespond()`，`xhttpd` 也会继续跟踪这个请求直到对应 future 完成
- 这样可以避免同一连接上的请求交错和 use-after-free

---

## 8. 建议继续阅读

- [XHTTPD API](../api/api-xhttpd.md)
- [Coroutine](../api/api-coroutine.md)
- [Future / Task / Promise](../api/api-future-task-promise.md)
