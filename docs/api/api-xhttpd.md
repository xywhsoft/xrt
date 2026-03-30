# XHTTPD HTTP/1.1 服务端主线

> 基于 `xnet-v2 + xnet_stream + xtlssession` 的当前 HTTP/1.1 服务端主线。

[返回索引](README.md)

---

## 1. 定位

`xhttpd` 是 XRT 当前内建的 HTTP/1.1 服务端层。

它负责：

- 监听与 accept
- 解析 HTTP/1.1 请求
- 物化 request / response 对象
- 支持明文 HTTP 与内建 TLS 服务路径
- 维护单连接上的串行 keep-alive 复用
- 与 runtime、future、task、coroutine 主线配合

`xhttpd` 的目标是“传输层 + handler 层”，而不是一个大而全的 Web 框架。

当前边界：

- request / response body 仍然是 whole-message 模型
- 静态文件、路由表、通用 upgrade 分发仍然 deferred
- 交互式终端、PTY 之类能力不属于这一层

---

## 2. 核心类型

### `xhttpdconfig`

重要字段：

- `tBindAddr`
- `iFlags`
- `iBacklog`
- `iRecvLimit`
- `pTlsConfig`

### `xhttpdrequest`

请求物化对象，包含：

- 请求行字段：`sMethod`、`sTarget`、`sPath`、`sQuery`、`sVersion`
- header 数组：`arrHeaders`
- body 指针与长度：`pBody / iBodyLen`
- `XHTTPD_REQ_F_KEEPALIVE` 等标记

### `xhttpdresponse`

响应物化对象，包含：

- 状态行字段：`iStatusCode`、`sReason`
- header 数组：`arrHeaders`
- body 指针与长度：`pBody / iBodyLen`
- `XHTTPD_RESP_F_CLOSE` 等关闭策略标记

### `xhttpdevents`

`xhttpd` 现在同时支持同步 handler 和异步 handler：

```c
typedef struct {
	void (*OnOpen)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn);
	bool (*OnRequest)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, xhttpdresponse* pResp);
	xfuture* (*OnRequestAsync)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq);
	void (*OnClose)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, xnet_result iReason);
	void (*OnError)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, int iSysErr);
} xhttpdevents;
```

请求分发优先级：

1. 如果设置了 `OnRequestAsync`，并且它返回非空 future，这个请求就进入异步路径。
2. 如果 `OnRequestAsync` 返回 `NULL`，且连接上还没有手动发送响应，则回退到 `OnRequest`。
3. 两条路径都没有处理时，`xhttpd` 自动返回 `404 Not Found`。

---

## 3. 正式 API

### 请求 / 响应辅助函数

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

### 异步连接辅助函数

```c
XXAPI bool xrtHttpdConnIsOpen(const xhttpdconn* pConn);
XXAPI xnet_result xrtHttpdConnRespond(xhttpdconn* pConn, const xhttpdresponse* pResp);
XXAPI xnet_result xrtHttpdConnClose(xhttpdconn* pConn, uint32 iCloseFlags);
```

这组 API 用来支持“先挂起请求，稍后再回包”的模式。

### 服务端生命周期

```c
XXAPI xhttpdserver* xrtHttpdCreate(xnetengine* pEngine, const xhttpdconfig* pCfg, const xhttpdevents* pEvents, ptr pUserData);
XXAPI uint16 xrtHttpdBoundPort(const xhttpdserver* pServer);
XXAPI xnet_result xrtHttpdStart(xhttpdserver* pServer);
XXAPI void xrtHttpdStop(xhttpdserver* pServer);
XXAPI void xrtHttpdDestroy(xhttpdserver* pServer);
```

---

## 4. 异步 handler 合同

### `OnRequest`

`OnRequest` 仍然是最简单的同步路径：

- 填充 `pResp`
- 返回 `true` 表示已处理
- 返回 `false` 表示交给内建 `404`

### `OnRequestAsync`

`OnRequestAsync` 是新的“请求可挂起、完成后再回包”路径。

推荐合同：

- 返回一个 `xfuture*`，表示该请求进入异步处理
- 返回 `NULL`，表示让 `xhttpd` 回退到 `OnRequest`
- future 成功完成且 value 非空时，这个 value 必须是 `xhttpdresponse*`
- future 成功完成且 value 为 `NULL` 时，表示 handler 已经通过 `xrtHttpdConnRespond()` 手动回包
- future 失败时，`xhttpd` 会映射成 `500`、`408` 或 `503` 这类错误响应

所有权规则：

- 返回给 `xhttpd` 的 future 所有权转移给 `xhttpd`
- future 成功产出的 `xhttpdresponse*` 所有权也转移给 `xhttpd`
- 异步场景建议用 `xrtHttpdResponseCreate()` / `xrtHttpdResponseDestroy()`

request 生命周期：

- `xhttpdrequest*` 会一直保持有效，直到这个异步请求真正完成，或连接被拆掉
- 如果你要跨越更长的异步边界，最稳的做法仍然是把自己需要的字段复制出来

connection 生命周期：

- 只要异步请求还在 pending，`xhttpd` 就会保住 `xhttpdconn`
- `xrtHttpdConnRespond()` 可以从别的线程、task 或 future 完成回调里调用
- keep-alive 仍然保持串行：当前异步请求没真正结束前，同一连接上的下一个请求不会继续推进

---

## 5. 推荐写法

### 同步 handler

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

### future 直接返回响应对象

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

其中 `procBuildResponse()` 返回 `XRT_NET_OK`，并把 `pOut->pValue` 设为 `xhttpdresponse*`。

### 手动延迟回包

```c
static xfuture* procOnRequestAsync(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq)
{
	(void)pOwner;
	(void)pServer;
	(void)pReq;
	return xTaskRunThread(procRespondLater, pConn, 0);
}
```

在 `procRespondLater()` 里：

- 创建 `xhttpdresponse*`
- 调用 `xrtHttpdConnRespond(pConn, pResp)`
- 销毁本地响应对象
- 最后把 future resolve 成 `NULL`

### coroutine 产出 future

`OnRequestAsync` 也可以返回 `xTaskRunCo()` 产出的 future。

常见做法是：

- 创建 scheduler
- 创建 coroutine future
- 跑一次 scheduler
- 把这个 future 返回给 `xhttpd`

如果你的服务主线本来就是 coroutine + future 组合，这条路径会很自然。

---

## 6. 说明与边界

- `xhttpd` 因为支持异步 handler，并不会自动变成一个路由框架。
- `xrtHttpdConnRespond()` 仍然是“发送一个完整响应消息”，不是 streaming body API。
- 如果你要做 websocket upgrade，请继续走专门的 `xws` 主线。
- 如果你要做大文件流式发送、range、middleware chain，这些仍然更适合放在上层。

---

## 7. 建议继续阅读

- [XNet V2](api-xnet-v2.md)
- [Network TLS](api-network-tls.md)
- [XHTTP](api-xhttp.md)
- [Coroutine](api-coroutine.md)
- [Future / Task / Promise](api-future-task-promise.md)
- [XHTTPD 异步处理入门](../guide/httpd-async-intro.md)
