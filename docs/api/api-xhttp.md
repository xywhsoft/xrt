# XHTTP HTTP/1.1 客户端主线

> 基于 `xnet-v2 + xtlssession` 的当前 HTTP/1.1 客户端主线。

[返回索引](README.md)

---

## 1. 定位

`xhttp` 是当前主线 HTTP 客户端。

它提供：

- 请求对象与响应对象
- 同步执行与异步执行
- 基于 `xnetengine` 的统一执行模型
- 对 `http / https` 的统一支持
- 同源串行 keep-alive 连接复用

它的设计目标不是做一个巨大而复杂的客户端框架，而是提供：

- 足够轻量
- 对基础设施友好
- 能和 `future / task / promise`
- 能和 `xnet-v2`
- 能和 AI / HTTP API 调用场景自然衔接


## 2. 核心类型

### `xhttprequest`

当前请求对象包含这些核心字段：

- `sMethod`
- `sURL`
- `tURL`
- `arrHeaders`
- `iHeaderCount`
- `pBody / iBodyLen`
- `iTimeoutMs`
- `bVerifyPeer`

它的设计特点是：

- 默认适合 whole-body 请求
- URL 既可保留原始串，也会解析到固定结构
- 头字段采用固定数组，避免过重依赖


### `xhttpresponse`

响应对象包含：

- `iStatusCode`
- `iFlags`
- `iHeaderCount`
- `iContentLength`
- `sVersion`
- `sReason`
- `arrHeaders`
- `pBody / iBodyLen`

当前主线的主要路径是：

- 收完整响应
- 再把响应对象交给上层消费


## 3. 当前正式 API

### 请求对象

```c
XXAPI void xrtHttpRequestInit(xhttprequest* pReq);
XXAPI void xrtHttpRequestUnit(xhttprequest* pReq);
XXAPI bool xrtHttpRequestSetMethod(xhttprequest* pReq, const char* sMethod);
XXAPI bool xrtHttpRequestSetURL(xhttprequest* pReq, const char* sURL);
XXAPI bool xrtHttpRequestSetHeader(xhttprequest* pReq, const char* sName, const char* sValue);
XXAPI bool xrtHttpRequestSetBodyCopy(xhttprequest* pReq, const void* pData, size_t iLen, const char* sContentType);
XXAPI void xrtHttpRequestSetTimeout(xhttprequest* pReq, uint32 iTimeoutMs);
XXAPI void xrtHttpRequestSetVerifyPeer(xhttprequest* pReq, bool bVerifyPeer);
```

推荐流程：

1. `RequestInit`
2. 设置 `Method / URL`
3. 按需设置 Header
4. 按需设置 Body
5. 设置超时与证书校验策略
6. 执行请求
7. `RequestUnit`


### 响应对象

```c
XXAPI void xrtHttpResponseDestroy(xhttpresponse* pResp);
XXAPI const char* xrtHttpResponseHeader(const xhttpresponse* pResp, const char* sName);
```

用途：

- 读取响应头
- 释放响应对象


### 执行 API

```c
XXAPI xnetfuture* xrtHttpExecuteAsync(xnetengine* pEngine, const xhttprequest* pReq);
XXAPI xhttpresponse* xrtHttpExecuteSync(xnetengine* pEngine, const xhttprequest* pReq, xnet_result* pStatus);
XXAPI void xrtHttpCloseIdleConnections(xnetengine* pEngine);
```

说明：

- `ExecuteAsync`：返回 future，适合和协程 / task / wait-source 主线组合
- `ExecuteSync`：同步等待一笔请求完成
- `CloseIdleConnections`：主动关闭空闲 keep-alive 连接


## 4. 连接与协议行为

当前 `xhttp` 的主路径特点是：

- 基于 `xnet_stream`
- 支持明文 HTTP 与内建 TLS
- 按 origin 进行串行 keep-alive 复用
- chunked transfer 已支持
- trailer 当前仍更偏元数据保留

这意味着：

- 普通 REST / JSON / LLM API 调用是当前最适合的使用场景
- 如果你要的是大规模流式 request/response body，当前还属于后续增强方向

## 5. 当前最推荐的使用场景

当前 `xhttp` 最适合这些场景：

- REST / JSON API
- 内部服务之间的 HTTP 调用
- 互联网 API 调用
- 非浏览器型的 LLM API 请求

也就是说，它当前更适合：

- whole-body 请求
- whole-body 响应
- 结构化数据交互

## 6. 常见用法

### 5.1 同步请求

```c
xhttprequest tReq;
xhttpresponse* pResp;
xnet_result iStatus;

xrtHttpRequestInit(&tReq);
xrtHttpRequestSetMethod(&tReq, "GET");
xrtHttpRequestSetURL(&tReq, "https://example.com/api/v1");
xrtHttpRequestSetTimeout(&tReq, 10000u);

pResp = xrtHttpExecuteSync(pEngine, &tReq, &iStatus);
if ( pResp != NULL ) {
	const char* sType = xrtHttpResponseHeader(pResp, "Content-Type");
	xrtHttpResponseDestroy(pResp);
}

xrtHttpRequestUnit(&tReq);
```


### 5.2 异步请求

```c
xnetfuture* pFuture;

pFuture = xrtHttpExecuteAsync(pEngine, &tReq);
/* 后续可通过 future / 协程等待 */
```


### 5.3 POST JSON

```c
xrtHttpRequestInit(&tReq);
xrtHttpRequestSetMethod(&tReq, "POST");
xrtHttpRequestSetURL(&tReq, "https://example.com/chat");
xrtHttpRequestSetHeader(&tReq, "Authorization", "Bearer ...");
xrtHttpRequestSetBodyCopy(&tReq, sJson, strlen(sJson), "application/json");
```


## 7. 与其他模块的关系

### 与 `xurl`

- 负责 URL 解析和 `Host` 头构建

### 与 `xhttp_util`

- 负责 header / token 等底层文本处理

### 与 `xtlssession`

- `https` 路径通过当前 TLS session 主线工作

### 与 `future / task / promise`

- `ExecuteAsync` 返回 future
- 可直接接入协程等待、组合等待、task group

### 与 AI 场景

- 当前 `xhttp` 是调用 LLM API 的正式 HTTP 主线入口
- 请求数据更推荐先用 `xvalue` 组织，再统一序列化为 JSON
- 更复杂的 timeout / cancel / 并发编排，则继续建立在 future / coroutine 主线上

## 8. 当前边界

当前 `xhttp` 文档覆盖的是：

- 当前 HTTP/1.1 客户端主线
- 请求/响应对象
- sync/async 执行模型

当前仍明确 deferred 的能力包括：

- 大规模流式 request body
- 大规模流式 response body
- 更复杂的连接池策略

## 9. 建议继续阅读

- [XNet V2](api-xnet-v2.md)
- [Network TLS](api-network-tls.md)
- [Future / Task / Promise](api-future-task-promise.md)
