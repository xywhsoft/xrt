# HTTP Server-Sent Events

`http_sse` 提供 WHATWG `text/event-stream` 的无状态封包层；
`http_sse_http` 提供通用 Header 与响应分类；`http_sse_parser` 在其上提供任意
分块增量解析。这三层都不绑定 HTTP 客户端、服务器或网络后端，现有
`xhttpbody`、HTTP Client Body 事件和 Server 流式响应可以直接组合这些原语。
`http_sse_client` 是可选的第四层，把公开 Parser、HTTP Client 和重定向能力组合成
完整 EventSource 会话；`http_sse_server` 是可选的第五层，把 Writer、规范响应字段
和通用有界 Body Stream 组合成服务端推送入口。两层都不复制 Header、URL、HTTP、
重连或网络状态机。

旧版 XRT 没有 SSE 专用实现、公开契约或测试。客户端和服务端会话是 2.0 新增的
组合层；它们复用的旧 HTTP 客户端、服务端和流式传输资产分别由对应 HTTP 模块
逐行审计，不在 SSE 模块下重复登记或重复实现。

## 裁剪

| 模块 | 宏 | 依赖 | 能力 |
| --- | --- | --- | --- |
| `http_sse` | `XHTTP_MODULE_HTTP_SSE` | `http`、`unicode` | 校验、计量、Event/Comment 封包 |
| `http_sse_http` | `XHTTP_MODULE_HTTP_SSE_HTTP` | `http_sse`、`http_headers`、`mime` | 请求/响应 Header 与状态分类 |
| `http_sse_parser` | `XHTTP_MODULE_HTTP_SSE_PARSER` | `http_sse`、`buffer` | 动态增量 Parser |
| `http_sse_client` | `XHTTP_CLIENT_MODULE_HTTP_SSE_CLIENT` | `http_sse_http`、`http_sse_parser`、`http_client_redirect` | EventSource 客户端、自动重连与背压 |
| `http_sse_server` | `XHTTP_SERVER_MODULE_HTTP_SSE_SERVER` | `http_sse_http`、`http_body_stream`、`http_server_reply` | SSE Reply、有界并发生产与事件发送 |

只发送 SSE 时不需要 Parser 和 Buffer。Parser 初始化不会预分配每连接缓冲；
`Line`、`Decoded`、`Data`、`Type`、`Id` 只按实际输入增长，
`xrtHttpSseParserTrim` 可以回收峰值容量。

对应的功能宏为：

```c
#define XHTTP_FEATURE_HTTP_SSE
#define XHTTP_FEATURE_HTTP_SSE_HTTP
#define XHTTP_FEATURE_HTTP_SSE_PARSER
#define XHTTP_FEATURE_HTTP_SSE_CLIENT
#define XHTTP_FEATURE_HTTP_SSE_SERVER
```

## 公共类型

事件标志、解析状态、项目类型和错误分类均为稳定枚举：

```c
typedef enum xhttpsseeventflag {
	XHTTP_SSE_EVENT_NONE = 0
} xhttpsseeventflag;

typedef enum xhttpsseparsestatus {
	XHTTP_SSE_PARSE_ERROR = -1,
	XHTTP_SSE_PARSE_MORE = 0,
	XHTTP_SSE_PARSE_ITEM = 1,
	XHTTP_SSE_PARSE_DONE = 2
} xhttpsseparsestatus;

typedef enum xhttpsseitemkind {
	XHTTP_SSE_ITEM_EVENT = 1,
	XHTTP_SSE_ITEM_COMMENT,
	XHTTP_SSE_ITEM_RETRY
} xhttpsseitemkind;

typedef enum xhttpsseerror {
	XHTTP_SSE_ERROR_ARGUMENT = 1,
	XHTTP_SSE_ERROR_STATE,
	XHTTP_SSE_ERROR_UTF8,
	XHTTP_SSE_ERROR_LINE_TOO_LARGE,
	XHTTP_SSE_ERROR_DATA_TOO_LARGE,
	XHTTP_SSE_ERROR_TYPE_TOO_LARGE,
	XHTTP_SSE_ERROR_ID_TOO_LARGE,
	XHTTP_SSE_ERROR_ALLOCATION
} xhttpsseerror;
```

`xhttpssemessage` 是 Parser 发布的借用消息视图；Parser 内部使用公开的
`xbuffer` 按需保存跨分块状态，不会为每个连接固定预留正文空间。

## 媒体类型

```c
#define XHTTP_SSE_MEDIA_TYPE "text/event-stream"
#define XHTTP_SSE_RETRY_DEFAULT UINT64_C(3000)
```

HTTP 响应应使用 `200` 和 `Content-Type: text/event-stream`。浏览器客户端只接受
UTF-8；`204 No Content` 表示停止自动重连。请求重连时可把
`xrtHttpSseParserLastEventId` 返回值设置为 `Last-Event-ID`。

## HTTP 适配

```c
bool xrtHttpSseContentTypeValid(xstrview ContentType);
bool xrtHttpSseRequestHeaders(
	xhttpheaders* pHeaders,
	xstrview LastEventId
);
bool xrtHttpSseResponseHeaders(xhttpheaders* pHeaders);
xhttpsseresponse xrtHttpSseResponseCheck(
	uint16 iStatus,
	const xhttpheaders* pHeaders
);
```

`xrtHttpSseContentTypeValid` 使用通用 MIME Parser，按 ASCII 大小写不敏感规则比较
`text/event-stream` 本体并严格验证全部参数。它是纯判断：不论匹配、类型不匹配
还是语法错误，都保留调用线程原有错误。

`xrtHttpSseRequestHeaders` 把 `Accept` 唯一设置为 `text/event-stream`；非空 ID
唯一设置为 `Last-Event-ID`，空 ID 删除全部同名字段。ID 必须是无 NUL、CR、LF
的合法 UTF-8。`xrtHttpSseResponseHeaders` 唯一设置 `Content-Type`。两个函数都
先修改完整副本再交换容器，因此验证、限额或 OOM 失败不会发布半组字段。

`xrtHttpSseResponseCheck` 的结果为：

- `XHTTP_SSE_RESPONSE_OPEN`：状态 200，且恰好一个合法 SSE Content-Type。
- `XHTTP_SSE_RESPONSE_STOP`：状态 204，调用方应停止自动重连。
- `XHTTP_SSE_RESPONSE_REJECT`：其他有效状态，或 200 的媒体类型缺失、重复、错误。
- `XHTTP_SSE_RESPONSE_ERROR`：状态码不在 100..999，线程错误为参数错误。

`REJECT` 和 `STOP` 是正常协议分类，不是库调用失败，因此保留调用线程原有错误；
只有 `ERROR` 会发布新的参数错误。`204` 和非 `200` 响应不读取 Header，调用方可以
传入空指针。

协议适配层不添加 `Cache-Control`、认证、连接管理等应用策略，也不要求使用
HTTP 客户端或服务器对象。调用方可直接操作通用 Header，也可由后续运行时薄
Helper 转接，固定封包、协议构建器和高级客户端/服务器路径互不排斥。

## 网络适配

EventSource 网络客户端在 [xhttp-client](../../../xhttp-client/docs/api/http_sse_client.md)，网络服务器在 [xhttp-server](../../../xhttp-server/docs/api/http_sse_server.md)。本库只编解码 SSE 与其 HTTP 消息。

## 事件封包

```c
typedef struct xhttpsseevent {
	xstrview Type;
	xstrview Data;
	xstrview Id;
	uint64 Retry;
	uint32 Flags;
} xhttpsseevent;
```

`Flags` 使用：

- `XHTTP_SSE_EVENT_DATA`
- `XHTTP_SSE_EVENT_TYPE`
- `XHTTP_SSE_EVENT_ID`
- `XHTTP_SSE_EVENT_RETRY`

标志区分字段省略和显式空值。显式空 `data` 会编码为 `data:\n\n`，接收端会发布
一条空消息；完全省略 `data` 的块只更新 ID 或 retry，不发布消息。

```c
bool xrtHttpSseEventValid(const xhttpsseevent* pEvent);
bool xrtHttpSseEventSize(const xhttpsseevent* pEvent, size_t* pSize);
bool xrtHttpSseEventWrite(
	const xhttpsseevent* pEvent,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);
str xrtHttpSseEventBuild(
	const xhttpsseevent* pEvent,
	size_t* pSize
);
```

`Write` 不附加零字符，空输出用于查询精确长度，容量不足不会写入部分事件。
`Build` 附加零字符并返回由 `xrtFree` 释放的字符串。封包使用规范 LF；`Data`
可以包含 LF 并自动拆成多条 `data` 行，但 CR 会被拒绝，以保证封包后数据精确
往返。Type 与 ID 必须是单行 UTF-8，ID 还禁止 NUL，因此可以安全复用为
`Last-Event-ID` Header。

事件固定描述符和所有长度输出都允许使用完整但未对齐的存储。实现先复制描述符，
验证借用视图、输出范围和别名关系，再一次性发布长度；地址回绕或输出覆盖事件、
字段视图时不会产生部分正文。

```c
bool xrtHttpSseLastEventIdValid(xstrview Id);
```

## 注释心跳

```c
bool xrtHttpSseCommentSize(xstrview Comment, size_t* pSize);
bool xrtHttpSseCommentWrite(
	xstrview Comment,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);
str xrtHttpSseCommentBuild(xstrview Comment, size_t* pSize);
```

Comment 中的 LF 会拆成多条 `:` 行。函数不添加事件分隔空行，因此可作为长连接
心跳直接发送；WHATWG 建议在可能经过旧代理的链路上周期性发送注释。

## Parser 配置

```c
typedef struct xhttpsseparserconfig {
	size_t LineLimit;
	size_t DataLimit;
	size_t TypeLimit;
	size_t IdLimit;
	uint64 Retry;
	xutfpolicy Utf8Policy;
	bool EmitComments;
	bool EmitRetry;
} xhttpsseparserconfig;
```

```c
void xrtHttpSseParserConfigInit(xhttpsseparserconfig* pConfig);
bool xrtHttpSseParserConfigValid(const xhttpsseparserconfig* pConfig);
```

默认限额为 64 KiB 单行、1 MiB 单事件数据、1 KiB 事件类型和 8 KiB ID。
默认 `XUTF_REPLACE` 按 UTF-8 最大无效子部件写入 U+FFFD，与浏览器解码行为一致；
`XUTF_STRICT` 在第一处无效字节返回协议错误。`EmitComments` 默认关闭，
`EmitRetry` 默认开启。

配置是调用时立即复制的固定值，允许位于完整但未对齐的存储中；调用返回后可以立即
修改或释放原配置。

## 生命周期

```c
bool xrtHttpSseParserInit(
	xhttpsseparser* pParser,
	const xhttpsseparserconfig* pConfig
);
xhttpsseparser* xrtHttpSseParserCreate(
	const xhttpsseparserconfig* pConfig
);
void xrtHttpSseParserUnit(xhttpsseparser* pParser);
void xrtHttpSseParserDestroy(xhttpsseparser* pParser);
void xrtHttpSseParserReset(xhttpsseparser* pParser);
void xrtHttpSseParserReconnect(xhttpsseparser* pParser);
bool xrtHttpSseParserTrim(xhttpsseparser* pParser);
```

`Reset` 建立全新的 EventSource 状态并清除 ID/retry；`Reconnect` 只开始一条新的
HTTP 响应，保留持久 ID 与 retry。失败和 EOF 都是当前响应的终态，必须调用其中
一个函数后才能继续解析。

`xhttpsseparser` 拥有五个动态缓冲，调用方栈分配的结构必须自然对齐，并且初始化后
不得直接修改公开内部字段。`Init`/`Unit` 配对用于调用方持有结构，`Create`/`Destroy`
配对用于堆对象，不能混用生命周期。

## 增量读取

```c
xhttpsseparsestatus xrtHttpSseParserRead(
	xhttpsseparser* pParser,
	xbytesview Input,
	bool bEnd,
	size_t* pConsumed,
	xhttpsseitem* pItem,
	xhttpsseerrorinfo* pError
);
```

返回值为 `ERROR`、`MORE`、`ITEM` 或 `DONE`。每次最多返回一个 Item，调用方按
`Consumed` 移除输入后继续调用，因此消费速度自然形成背压，不需要 Parser 建立
第二条消息队列。

`Consumed`、`Item` 和可选 `Error` 支持完整但未对齐的输出存储，结果由对齐局部值
一次性发布。非法别名、地址回绕或输出位于 Parser 动态缓冲中时，Parser 与全部输出
保持不变。累计字节偏移和行号在 `size_t` 极限处饱和，不会在超长连接中回绕。

Item 类型：

- `XHTTP_SSE_ITEM_EVENT`：读取 `Item.Message`；空 Type 已映射为静态 `message`。
- `XHTTP_SSE_ITEM_COMMENT`：仅在 `EmitComments` 开启时读取 `Item.Comment`。
- `XHTTP_SSE_ITEM_RETRY`：仅在 `EmitRetry` 开启且值是非空十进制时读取 `Item.Retry`。

Message、Comment 和 LastEventId 都借用 Parser 内存，只保证到下一次修改 Parser
的调用。EOF 不会发布缺少最终空行的事件。解析器接受 CRLF、单 CR、单 LF，并只
移除流开头的一个 UTF-8 BOM。字段名称按规范区分大小写，未知字段和无效 retry
被忽略，包含 NUL 的 `id` 不更新持久 ID。

```c
xstrview xrtHttpSseParserLastEventId(const xhttpsseparser* pParser);
uint64 xrtHttpSseParserRetry(const xhttpsseparser* pParser);
```

## 示例

```c
xhttpsseevent Event = {
	XRT_STR_INIT("progress"),
	XRT_STR_INIT("{\"percent\":75}"),
	XRT_STR_INIT("job-42:3"),
	2000,
	XHTTP_SSE_EVENT_TYPE |
		XHTTP_SSE_EVENT_DATA |
		XHTTP_SSE_EVENT_ID |
		XHTTP_SSE_EVENT_RETRY
};
str sWire = xrtHttpSseEventBuild(&Event, NULL);

/* sWire 可以直接作为一次流式 HTTP Body 数据块发送。 */
xrtFree(sWire);
```

封包示例位于 `examples/http/sse/main.c`，通用 HTTP 适配示例位于
`examples/http/sse_http/main.c`，自动重连客户端示例位于
`examples/http/sse_client/main.c`，服务端有界生产示例位于
`examples/http/sse_server/main.c`。

回归覆盖 GCC/TCC 模块化与单头文件、裁剪依赖负例、逐分配点 OOM 原子性、
任意分块确定性 Fuzz、Writer 到 Parser 的精确往返，以及客户端暂停/恢复、重入关闭、
204、拒绝、断线重连、Last-Event-ID、最终重定向 URL 持久化，以及服务端 Reply、
事件原子提交、反压和创建/发送 OOM 回滚。服务端运行时回归还通过 Select、本机
随机端口、真实 HTTP Client/Server 和三字节
短写验证响应提交后的跨线程生产、chunked 解帧及正常 EOF。LibFuzzer 与 Sanitizer
目标由 `tools/test_protocol_fuzz.py sse` 在 Clang 环境运行。

## 协议依据

- WHATWG HTML Living Standard 9.2 Server-sent events。
- 事件流固定 UTF-8，媒体类型为 `text/event-stream`。
- 一个前导 BOM 被移除；CRLF、CR 和 LF 都是行结束。
- `data` 多行以 LF 合并，最终 LF 在发布前移除。
- EOF 丢弃没有空行结束的事件。
