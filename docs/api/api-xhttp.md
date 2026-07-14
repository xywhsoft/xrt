# XHTTP HTTP/1.1 客户端

`xhttp` 是 XRT 基于 `xnet-v2` 与 `xtlssession` 实现的 HTTP/1.1 客户端。本阶段明确不实现 HTTP/2。

[返回索引](README.md)

## 能力范围

- 明文 HTTP 与内置 TLS
- 直连、SOCKS5 和 HTTP CONNECT 传输
- 缓冲请求体
- 缓冲接收或增量回调响应体
- `Content-Length`、chunked 和连接关闭定界的响应
- 总超时、空闲超时与显式取消
- 可动态增长的请求头和响应头集合
- 兼容 API 使用的进程级连接池，以及可独立管理的客户端连接池

## 请求与响应的所有权

栈上请求必须通过 `xrtHttpRequestInit()` 初始化，并通过 `xrtHttpRequestUnit()` 释放。请求对象拥有复制后的请求体、扩展的请求头存储，以及它持有的代理对象引用。

成功完成的 future 不会自动销毁响应。调用方取得 `xhttpresponse*` 后，必须调用 `xrtHttpResponseDestroy()`。

流式执行返回的最终响应只保存状态码和响应头等元数据。此时 `pBody` 为 `NULL`，`iBodyLen` 记录已通过 `OnBody` 交付的解码后字节总数。

请求对象初始化后不要按值复制或使用 `memcpy` 复制；需要创建独立请求时，应重新初始化并通过公开 API 设置字段。

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

`SetHeader` 按大小写不敏感匹配并替换第一个同名项；`AddHeader` 保留 `Set-Cookie` 等合法重复项；`RemoveHeader` 删除全部同名项。

## 缓冲执行

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
	/* 使用 resp->pBody */
	xrtHttpResponseDestroy(resp);
}
xrtHttpRequestUnit(&req);
```

`xrtHttpExecuteAsync()` 使用相同传输路径，并返回 `xnetfuture*`。

## 流式执行

回调运行在连接对应的网络工作线程中。传给 `OnBody` 的数据指针只在本次回调期间有效。任一回调返回 `false` 都会取消请求。

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

chunked 分帧会在调用 `OnBody` 前移除。响应 trailer 当前会被正确消费，但尚未作为响应元数据公开。

## 取消

```c
if ( !xrtHttpCancel(future) ) {
	/* 请求可能已经完成 */
}
```

取消操作会中断活动连接，并以 `XRT_NET_CANCELLED` 完成 future。缓冲执行和流式执行都支持取消。

## 独立客户端与连接池

建议为一个长期存活的模型提供方或服务边界创建一个 `xhttpclient`。它绑定一个 `xnetengine`，并独立拥有 keep-alive 连接池。

```c
xhttpclient* client = xrtHttpClientCreate(engine);

future = xrtHttpClientExecuteStreamAsync(client, &req, &callbacks);
/* 等待并释放 future 和响应 */

xrtHttpClientCloseIdle(client);
xrtHttpClientDestroy(client);
```

客户端销毁时，尚在完成异步清理的连接会通过内部引用保证对象生命周期安全。调用 `xrtHttpClientDestroy()` 后不得再发起新请求。

兼容 API `xrtHttpExecuteAsync()`、`xrtHttpExecuteSync()` 和 `xrtHttpCloseIdleConnections()` 继续使用进程级默认连接池。

## 当前边界

- 请求体在发送前完整缓冲
- 响应 trailer 尚未公开
- 每条 HTTP/1.1 连接同一时刻只有一个请求，不实现 pipelining
- 本阶段不计划实现 HTTP/2

## 相关模块

- [XNet V2](api-xnet-v2.md)
- [Network TLS](api-network-tls.md)
- [Future / Task / Promise](api-future-task-promise.md)
