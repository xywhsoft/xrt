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

- request body 可选择缓冲或通过 body-stream 回调消费
- response 可选择缓冲、流式发送或文件发送
- 路由与 middleware 由更高层的 `xweb` 提供
- 交互式终端、PTY 之类能力不属于这一层

---

## 2. 核心类型

### `xhttpdconfig`

重要字段：

- `iSize` / `iVersion`
- `tBindAddr`
- `iFlags`
- `iBacklog`
- `iRecvLimit`
- `pTlsConfig`

该结构必须先通过 `xrtHttpdConfigInit()` 初始化。`xhttpdconfig` 与
`xhttpdevents` 都采用 size/version 前缀，使未来运行库能够识别旧版前缀并为新增字段使用默认值。事件表应使用 `xrtHttpdEventsInit()` 初始化，不再依赖裸 `memset`。

### `xhttpdrequest`

请求物化对象，包含：

- 请求行字段：`sMethod`、`sTarget`、`sPath`、`sQuery`、`sVersion`
- 可动态增长的 header 表：`arrHeaders / pHeaders`，名称和值按实际长度保存
- 可动态增长的 trailer 表，名称和值按实际长度保存
- body 指针与长度：`pBody / iBodyLen`
- `XHTTPD_REQ_F_KEEPALIVE` 等标记

### `xhttpdresponse`

响应物化对象，包含：

- 状态行字段：`iStatusCode`、`sReason`
- 不受固定字段长度限制的动态 reason phrase
- 可动态增长的 header 表：`arrHeaders / pHeaders`，名称和值按实际长度保存
- body 指针与长度：`pBody / iBodyLen`
- 可动态增长的响应 trailer 表
- `XHTTPD_RESP_F_CLOSE` 等关闭策略标记

### `xhttpdevents`

`xhttpd` 现在同时支持同步 handler 和异步 handler：

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

请求分发优先级：

1. 如果设置了 `OnRequestAsync`，并且它返回非空 future，这个请求就进入异步路径。
2. 如果 `OnRequestAsync` 返回 `NULL`，且连接上还没有手动发送响应，则回退到 `OnRequest`。
3. 两条路径都没有处理时，`xhttpd` 自动返回 `404 Not Found`。

---

## 3. 正式 API

### 请求 / 响应辅助函数

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

请求 header、请求 trailer、响应 header 和响应 reason phrase 不再受旧版 `XHTTPD_*_CAP` 固定数组限制。`xrtHttpdRequestUnit()`、`xrtHttpdResponseUnit()` 和 `xrtHttpdResponseDestroy()` 会释放动态字符串；不要按值复制已初始化对象，也不要直接改写字段项，应使用公开 API。

### HTTP/1.1 校验与响应语义

服务端在进入 handler 前拒绝有歧义的分帧、冲突的 `Content-Length`、不支持的传输编码、错误 chunk、非法字段和禁止的 trailer。HTTP/1.1 必须恰好包含一个合法的 `Host` authority；HTTP/1.0 允许零个或一个。唯一且合法的 `Expect: 100-continue` 会在读取正文前收到临时响应，不支持或重复的 Expect 返回 `417 Expectation Failed`。

响应序列化严格执行方法与状态码语义：`HEAD` 发送与 `GET` 相同的 representation 元数据但不发送正文；`1xx` 与 `204` 删除正文分帧字段和正文；`304` 不发送正文，但保留显式指定的 representation `Content-Length`；空 chunked 响应只发送一次终止 chunk。

响应 trailer 使用拥有所有权的副本，并自动采用 HTTP/1.1 chunked 分帧。序列化器会在需要时生成 `Transfer-Encoding: chunked` 和准确的 `Trailer` 声明。HTTP 禁止的 trailer 字段、无正文状态码或 `HEAD` 的 trailer，以及与 `Content-Length` 同时出现的 trailer 都会被拒绝。`xrtHttpdConnStart()` 会复制并预构建终止块，因此 `Start` 返回后即可销毁源 response。

### 异步连接辅助函数

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

`xrtHttpdConnUpgrade()` 用于同步协议交接。响应必须是无正文的 `101`，并包含有效的 `Connection: Upgrade` 与 `Upgrade` 字段；请求本身也必须是无正文的 Upgrade 请求。该函数只能在当前同步 handler 中、对应 stream 的 worker 上调用。成功后，返回 stream 的所有权转移给调用方，stream 从 HTTP 服务端连接集合中移除，服务端停止时也不会再由 HTTP 层关闭它。新协议不会再次收到 `OnOpen`；HTTP 头之后已经收到的字节会在 handler 返回后交给新的 `OnRecv`。调用方最终必须关闭并销毁 stream。

xweb 层通过 `xrtWebResponseSetTrailer()`、`xrtWebResponseAddTrailer()`、trailer 访问器以及 `xrtWebResponseUpgrade()` 提供同等契约，无需暴露 `xhttpdconn`。

xweb response 一旦通过 `Start`、sendfile 或 Upgrade 提交，所有构建型操作都拒绝修改：状态设置变为无操作，header、trailer、缓冲 body、Cookie 与 redirect 设置返回失败。middleware 仍可观察已提交响应，但不能重写已经交给传输层的字节。

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
- `xrtHttpdRequestContext()` 返回借用指针，跨异步边界前必须 retain
- 请求完成、对端断开或服务停止时，该 Context 会被取消

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
- `xrtHttpdConnRespond()` 发送完整响应；流式正文使用 `Start` / `Send` / `End`。
- `xrtHttpdConnUpgrade()` 提供协议无关的 stream 交接；`xws` 在该契约之上实现 WebSocket 握手策略与帧协议。
- 路由、multipart 表单策略、协商式响应压缩和 middleware 属于 `xweb`。

### xweb middleware 契约

```c
XXAPI bool xrtWebAppUseEx(xwebapp* pApp, xwebmiddleware pMiddleware,
    ptr pUserData, xwebfree pFreeUserData);
XXAPI bool xrtWebAppUse(xwebapp* pApp, xwebmiddleware pMiddleware, ptr pUserData);
XXAPI bool xrtWebServerUseEx(xwebserver* pServer, xwebmiddleware pMiddleware,
    ptr pUserData, xwebfree pFreeUserData);
XXAPI bool xrtWebServerUse(xwebserver* pServer, xwebmiddleware pMiddleware, ptr pUserData);
XXAPI xwebaction xrtWebNext(xwebnext* pNext);
```

middleware 按注册顺序包围路由、静态文件和内建 404 分派。`xrtWebNext()` 是同步、仅在当前回调内有效且最多调用一次的对象。middleware 未调用 next 而返回 `XWEB_NEXT` 时运行库自动继续；调用 next 后再返回 `XWEB_NEXT` 会保留下游 action。`XWEB_DONE` 立即短路；`XWEB_ERROR` 会先丢弃未提交的部分响应，再生成 500。每个请求在开始分派时固定 middleware 数量快照，并发注册只影响后续请求。

通用 middleware 不包围流式 request-body 回调。流式路由使用独立的 `Begin / Body / End` 生命周期，并应在这些回调中完成认证、准入和 multipart 存储策略。同步 middleware/handler 链返回未提交的 `XWEB_ASYNC` 会得到 500；异步服务逻辑应使用显式的 HTTPD future 路径或异步流式 body-end handler。

### xweb multipart 与响应压缩策略

缓冲的 `multipart/form-data` 请求会在进入路由前通过流式 multipart 状态机严格校验。缺少或非法 boundary、截断正文、非法或重复的 part 语义字段、缺少 form-data `name` 都返回 `400`；part 数量、part 正文大小、part header 数量和 header 字节数超限返回 `413`。显式的流式正文路由接收原始 chunk，并由业务决定 multipart 的落盘与存储策略。

xweb 的缓冲响应可通过 `iCompressionFlags` 启用 gzip/deflate，`iCompressionMinBytes`、`iCompressionMaxBytes` 和 `iCompressionLevel` 用于限制 CPU 与内存成本。协商遵守 `Accept-Encoding` q 值、wildcard 和 identity 语义，生成 `Vary: Accept-Encoding`，没有可接受编码时返回 `406`。`Cache-Control: no-transform`、`Content-Range`、已有 `Content-Encoding`、不可压缩媒体类型、流式响应和 sendfile 路径不会自动压缩。

---

## 7. 建议继续阅读

- [XNet V2](api-xnet-v2.md)
- [Network TLS](api-network-tls.md)
- [XHTTP](api-xhttp.md)
- [Coroutine](api-coroutine.md)
- [Future / Task / Promise](api-future-task-promise.md)
- [XHTTPD 异步处理入门](../guide/httpd-async-intro.md)
