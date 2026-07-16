# XHTTP HTTP/1.1 客户端

`xhttp` 是 XRT 基于 `xnet-v2` 与 `xtlssession` 实现的 HTTP/1.1 客户端。本阶段明确不实现 HTTP/2。

[返回索引](README.md)

## 能力范围

- 明文 HTTP 与内置 TLS
- 直连、SOCKS5 和 HTTP CONNECT 传输
- 复制或流式请求体
- 缓冲接收或增量回调响应体
- `Content-Length`、chunked 和连接关闭定界的响应
- 总超时、空闲超时与显式取消
- 可动态增长的请求头和响应头集合
- 动态保存方法、URL、Host、请求目标、reason phrase 和 trailer
- 严格 HTTP/1.1 分帧校验与可配置解析限额
- 兼容 API 使用的进程级连接池，以及可独立管理的客户端连接池

## 请求与响应的所有权

栈上请求必须通过 `xrtHttpRequestInit()` 初始化，并通过 `xrtHttpRequestUnit()` 释放。请求对象拥有复制后的请求体、扩展的请求头存储、每个 header 的完整名称和值，以及它持有的代理对象引用。

成功完成的 future 不会自动销毁响应。调用方取得 `xhttpresponse*` 后，必须调用 `xrtHttpResponseDestroy()`。

流式执行返回的最终响应只保存状态码和响应头等元数据。此时 `pBody` 为 `NULL`，`iBodyLen` 记录已通过 `OnBody` 交付的解码后字节总数。

请求对象初始化后不要按值复制、使用 `memcpy` 复制或直接改写 header 项；需要创建独立请求时，应重新初始化并通过公开 API 设置字段。

## 自动重定向

`xrtHttpClientConfigInit()` 默认启用自动重定向，最多跟随 10 跳。策略由 `iFlags` 与 `iMaxRedirects` 控制：

- `XHTTP_CLIENT_F_FOLLOW_REDIRECTS` 跟随带有唯一 `Location` 字段的 301、302、303、307 和 308 响应。
- `XHTTP_CLIENT_F_REDIRECT_POST_TO_GET` 对 301/302 应用常见的 POST 转 GET 规则；303 总是转为 GET，HEAD 仍保持 HEAD。改写方法时会删除正文及其 framing/content header。
- 307/308 保留方法与正文。流式正文必须声明 `XHTTP_BODY_F_REPLAYABLE`，并且每一跳都会重新打开 reader。
- 跨源跳转会删除显式 `Cookie`；未设置 `XHTTP_CLIENT_F_ALLOW_CROSS_ORIGIN_AUTH` 时，还会删除 `Authorization` 与 `Proxy-Authorization`。CookieJar 会针对目标 URL 重新匹配。
- HTTPS 降级到 HTTP 默认以 `XHTTP_ERROR_REDIRECT_DOWNGRADE` 失败，只有设置 `XHTTP_CLIENT_F_ALLOW_HTTPS_DOWNGRADE` 才允许。

总 deadline 与取消上下文覆盖完整重定向链。中间响应正文会被排空以便复用连接，但不会进入流式回调。`xrtHttpResponseURL()` 返回最终有效 URL，`xhttpdiagnostics.iRedirectCount` 返回实际跟随的跳数。

## 响应自动解压契约

`xrtHttpClientConfigInit()` 默认启用 `XHTTP_CLIENT_F_AUTO_DECOMPRESS`。调用方没有设置 `Accept-Encoding` 时，客户端自动发送 `Accept-Encoding: gzip, deflate`；调用方显式设置的值保持不变。

对于最终且允许携带正文的响应，客户端支持 `gzip`、兼容名称 `x-gzip` 和 `deflate`。多层 `Content-Encoding` 按应用顺序的逆序解码，最多支持四层非 `identity` 编码；`deflate` 同时兼容规范的 zlib wrapper 与旧服务端常见的 raw deflate。未知、格式错误或层数过多的编码列表保持原始正文和 header，不进行猜测。重定向中间响应及无正文响应不执行解压。

解压启用后，缓冲正文和流式 `OnBody` 都接收解码后的字节。响应设置 `XHTTP_RESP_F_DECOMPRESSED`，移除公开视图中的 `Content-Encoding` 和编码前 `Content-Length`，完成后的 content length 是解码字节数。`xrtHttpResponseOriginalContentEncoding()` 返回被移除的完整编码值；该指针为借用引用，在 `xrtHttpResponseDestroy()` 前有效。

压缩正文和解压正文分别受 `tHttp1Limits.iMaxBodyBytes` 约束。压缩流格式错误、截断、校验和错误或解压后超限时，请求以 `XHTTP_ERROR_DECOMPRESSION` 失败，不会把部分解码数据报告为成功响应。定义 `XRT_NO_XINFLATE` 会移除解压器、关闭默认标志，并使 `xrtHttpClientCreateEx()` 拒绝显式启用自动解压的配置。

诊断中的 `iResponseBodyBytes` 表示最终响应已解码或已交付的正文长度，`iResponseWireBodyBytes` 表示该响应在 HTTP 线上收到的编码正文长度。客户端统计中的 `iResponseBodyBytes` 统计所有重定向跳的编码正文总量。

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

`SetHeader` 按大小写不敏感匹配并替换第一个同名项；`AddHeader` 保留 `Set-Cookie` 等合法重复项；`RemoveHeader` 删除全部同名项。

认证辅助函数会替换唯一的 `Authorization` 字段。通用函数要求认证 scheme 是合法 HTTP token，并拒绝非法字段值；Basic 按原始 `user:password` 字节编码，用户名中不得包含冒号；Bearer 将 token 作为不透明字段数据处理。重定向默认只在同源跳转中保留认证信息，除非显式允许跨源携带认证。

Header 名称和值按实际长度动态保存；方法、URL、解析后的 Host 与请求目标、响应 reason phrase 和 trailer 也遵循相同规则。它们不再受旧版 `XHTTP_*_CAP` 固定数组限制，也不会静默截断。请求字符串由请求对象持有到 `xrtHttpRequestUnit()`；响应字符串由响应对象持有到 `xrtHttpResponseDestroy()`。旧容量宏仅保留用于源码兼容，不表示运行时长度上限。

## HTTP/1.1 分帧合同

接收路径拒绝有歧义或非法的消息分帧，包括同时出现 `Transfer-Encoding` 与 `Content-Length`、重复但不一致的 `Content-Length`、obs-fold、非法字段名或字段值、不支持的传输编码、错误 chunk 格式以及禁止出现在 trailer 中的字段。重复或逗号分隔的 `Content-Length` 只有在每个值完全相同时才允许。

请求发送前执行同样的元数据检查：显式 `Content-Length` 必须与已缓冲正文长度完全一致，不得与 `Transfer-Encoding` 共存，并且客户端只生成受支持的最终 `chunked` 编码。非法请求元数据报告 `XHTTP_ERROR_INVALID_ARGUMENT`，分配失败报告 `XHTTP_ERROR_OUT_OF_MEMORY`。

使用 `xrtHttpClientConfigInit()` 初始化 `xhttpclientconfig` 后，可以覆盖 `tHttp1Limits` 与 `iMaxInformationalResponses`，再传给 `xrtHttpClientCreateEx()`。这些配置分别限制起始行、header 总字节数、单 header 行、header 数量、正文、chunk 行和 trailer 数量；`1xx` 响应在最终响应前被消费，并受独立数量上限保护。

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

chunked 分帧会在调用 `OnBody` 前移除。解析后的 trailer 会附加到最终响应，可在执行完成后通过 trailer accessor 读取。

## 取消

```c
if ( !xrtHttpCancel(future) ) {
	/* 请求可能已经完成 */
}
```

取消操作会中断活动连接，并以 `XRT_NET_CANCELLED` 完成 future。缓冲执行和流式执行都支持取消。

## 单请求诊断

当上层 provider 需要稳定的失败分类和耗时数据时，可在执行前绑定一个由调用方持有的 `xhttpdiagnostics`：

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

诊断对象采用借用语义，生命周期必须覆盖整个异步请求。future 完成后，其中包含最终传输状态、系统错误、总超时与空闲超时的区分、取消、协议/回调错误、连接复用情况、单调时钟里程碑、派生耗时和字节数。成功响应也保存同一份最终快照，可通过 `xrtHttpResponseDiagnostics()` 读取。

XRT 只报告传输事实，不隐式重试请求。重试和幂等策略由调用方（例如 LLM provider 适配器）负责。

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

`xrtHttpClientConfigInit()` 默认创建有界连接池：客户端总计最多保留 128 条空闲连接，每个源最多 8 条，空闲 90 秒后过期。`iMaxIdleConnections` 是客户端总上限；`iMaxIdleConnectionsPerOrigin` 是同一传输源（scheme、host、port、proxy、TLS 校验策略与 engine）的上限；`iIdleConnectionTimeoutMs` 控制空闲过期。任一容量设为 0 都会禁用连接复用。仅将超时设为 0 时，连接池仍受容量约束，连接会在复用失败、容量淘汰、显式关闭、对端关闭或客户端销毁时释放。达到上限后，连接池淘汰适用范围内最旧的空闲连接。

可以通过统计快照观察连接池和客户端生命周期：

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

当前活动/空闲连接数是并发快照。其余计数在客户端生命周期内单调递增，包括开始/完成的请求、成功打开的连接、池复用、关闭、实际跟随的重定向、请求线缆字节和所有重定向跳的响应正文总字节。连接关闭清理是异步的，因此 `iConnectionsClosed` 可能在请求 future 完成后稍晚递增。

兼容 API `xrtHttpExecuteAsync()`、`xrtHttpExecuteSync()` 和 `xrtHttpCloseIdleConnections()` 继续使用进程级默认连接池。

## 当前边界

- 请求体可以复制，也可以通过可重放或不可重放的 `xhttpbody` 工厂流式发送
- 每条 HTTP/1.1 连接同一时刻只有一个请求，不实现 pipelining
- 本阶段不计划实现 HTTP/2

## 相关模块

- [XNet V2](api-xnet-v2.md)
- [Network TLS](api-network-tls.md)
- [Future / Task / Promise](api-future-task-promise.md)
