# HTTP 协议底座

XRT 的 HTTP 模块是可直接组合 TCP、TLS 和自定义传输的 HTTP/1 线协议底座，
不提供客户端对象、服务器对象、路由、中间件或请求/响应拥有型模型。

核心目标是：严格解析不可信输入、完整表达 HTTP/1.0 和 HTTP/1.1 分帧、允许
调用方直接发送原始报文，并保证常用路径不分配正文缓冲。

完整函数、常量和类型索引见 [HTTP 公共符号参考](http-reference.md)。精确参数、所有权和
失败契约以对应公共头的中文注释为准。

## 模块边界

| 模块 | 裁剪宏 | 作用 |
| --- | --- | --- |
| `http` | `XRT_MODULE_HTTP` | token、字段、方法、状态和 Content-Length |
| `http_param` | `XRT_MODULE_HTTP_PARAM` | 参数与 quoted-string 语法 |
| `http_param_host` | `XRT_MODULE_HTTP_PARAM_HOST` | 参数解码后的无分配 Host 验证 |
| `http_expect` | `XRT_MODULE_HTTP_EXPECT` | Expect 字段 |
| `http_upgrade` | `XRT_MODULE_HTTP_UPGRADE` | Upgrade 字段与写出 |
| `http_te` | `XRT_MODULE_HTTP_TE` | TE 与 transfer-coding 参数 |
| `http_connection` | `XRT_MODULE_HTTP_CONNECTION` | Connection 选项与持久连接 |
| `http_trailer` | `XRT_MODULE_HTTP_TRAILER` | Trailer 声明和尾字段约束 |
| `http_encoding` | `XRT_MODULE_HTTP_ENCODING` | Accept-Encoding 与 Content-Encoding |
| `http_decode` | `XRT_MODULE_HTTP_DECODE` | 流式 gzip/deflate 自动解码 |
| `http_host` | `XRT_MODULE_HTTP_HOST` | Host authority |
| `http_target` | `XRT_MODULE_HTTP_TARGET` | 四种 request-target |
| `http1_head` | `XRT_MODULE_HTTP1_HEAD` | 起始行、Header 解析与写入 |
| `http1_net` | `XRT_MODULE_HTTP1_NET` | TCP 块链 Header 解析与余量保留 |
| `http1_tls` | `XRT_MODULE_HTTP1_TLS` | TLS 明文块链增量 Header 解析 |
| `http1_body` | `XRT_MODULE_HTTP1_BODY` | 正文计划、chunked 和 trailer |
| `http1_message` | `XRT_MODULE_HTTP1_MESSAGE` | 连续内存完整消息便利层 |

`http_decode` 才依赖 Inflate，`http1_net` 才依赖网络块链，`http1_tls` 才依赖 TLS Stream。
只启用连续内存 HTTP/1 解析不会带入压缩、网络、TLS 或 WebSocket。

## 借用模型

`xhttpfield`、`xhttp1head`、`xhttp1message` 和所有语法游标都借用调用方输入。
解析器不复制字段名称和值，输入缓冲和字段描述符数组必须覆盖借用视图的使用期。

请求方法同时以原始 `xhttp1head.Method` 视图和 `xhttp1head.MethodCode` 枚举发布。
`xrtHttpMethodParse` 按大小写敏感规则识别 GET、HEAD、POST、PUT、DELETE、CONNECT、
OPTIONS、TRACE 和 PATCH；合法但未内置的方法返回 `XHTTP_METHOD_OTHER`，非法 token
返回 `XHTTP_METHOD_INVALID`。每个非零枚举值占用独立 bit，既可以作为实际方法直接
比较，也可以通过位或组合成路由方法集合。`XHTTP_METHOD_CRUD` 覆盖 GET、POST、PUT、
PATCH 和 DELETE，`XHTTP_METHOD_ANY` 覆盖全部内置方法及 `OTHER`；应用仍应保留原始
视图处理具体的扩展方法。

字段描述符允许位于合法的未对齐存储。实现使用安全加载，不要求调用方为了协议
解析调整缓冲布局。

XRT 不再提供动态 Header 容器。需要拥有、修改或索引 Header 的框架应在 `xhttp`
中建立对象模型；快速路径可以直接使用栈数组或应用自己的存储。

## Host 语法

`xrtHttpHostParse` 保留 Host、可选端口和 IP-literal 分类，任意长度十进制端口在
协议层仍然合法，只有可放入 `uint16` 的值才带 `XHTTP_AUTHORITY_PORT_VALUE`。
`xrtHttpIpv4Valid` 与 `xrtHttpIpv6Valid` 是无错误副作用的纯谓词，公开同一套严格
IP 文本规则，扩展协议无需复制 Host 内部解析器。

`xrtHttpParamHostValid` 直接消费 `xhttpparam` 的语义值。quoted-pair 会在验证时
流式解码，任意长度 reg-name 与 IPvFuture 不进入固定缓冲，也不申请堆内存。

## Header 解析

`xrtHttp1RequestParse` 和 `xrtHttp1ResponseParse` 接受从消息首字节开始的累计输入：

- `XHTTP1_MORE`：输入尚未包含完整 Header；
- `XHTTP1_FIELDS`：Header 完整，但字段描述符容量不足，`FieldCount` 给出需求；
- `XHTTP1_READY`：解析完成；
- `XHTTP1_ERROR`：协议、限额或参数错误。

解析器只接受 CRLF，拒绝 obs-fold、裸 LF、非法字段名、控制字符、冲突的
Content-Length、歧义 Transfer-Encoding 和非法 Upgrade。`xhttp1errorinfo`
提供稳定错误码、消息相对偏移和行号。

`xhttp1limits` 分别限制 Header 总长度、起始行、单字段行和字段数量。默认值面向
公网输入，服务端应按路由策略进一步收紧，而不是无上限增长接收缓冲。

网络热路径使用 `xrtHttp1RequestParseBuffer`、`xrtHttp1ResponseParseBuffer` 直接扫描
`xnetbuf`。只有完整 Header 跨块时才连续化实际 Header 前缀，函数不消费输入，Upgrade 后的余量
保持原位。TLS 对应入口是 `xrtHttp1RequestParseTls`、`xrtHttp1ResponseParseTls`；输入不足时它们
通过 `xrtTlsStreamReadMore` 请求下一段受限明文，Header 完成后仍由调用方消费 `Head.Bytes`。

## 正文计划

Header 完成后必须调用：

- `xrtHttp1RequestBodyPlan` 处理请求；
- `xrtHttp1ResponseBodyPlan` 处理响应，并传入原请求方法。

`xhttp1bodyplan` 的模式是唯一分帧结论：

- `XHTTP1_BODY_NONE`：没有正文；
- `XHTTP1_BODY_FIXED`：Content-Length 定长；
- `XHTTP1_BODY_CHUNKED`：分块编码；
- `XHTTP1_BODY_CLOSE`：由可靠 EOF 定界；
- `XHTTP1_BODY_TUNNEL`：CONNECT 或 101 后的字节不再属于 HTTP。

响应计划覆盖 HEAD、成功 CONNECT、1xx、204 和 304。调用方不应仅凭
Content-Length 判断响应正文。

## 流式接收

`xrtHttp1BodyInit` 创建无分配 reader。`xrtHttp1BodyRead` 每次返回一个借用正文
片段、容量请求或终态，并通过 `Consumed` 精确说明已消费线路字节。

`xrtHttp1BodyLimitsInit` 的 `MaxBody` 默认为 `UINT64_MAX`，因为底层 reader 不拥有、
聚合或分配正文。服务端、代理以及任何会持有正文的上层必须在接收前按路由和内存预算
设置有限上限，不能把该协议层默认值直接作为公网接收策略。

对于 chunked，reader 会验证 chunk-size、扩展语法、CRLF、last-chunk 和 trailer，
并严格拒绝 chunk-size 与分号或 CRLF 之间的空白，避免端点对报文边界产生分歧。
正文视图不包含分块元数据。`Received` 是应用正文长度，`WireBytes` 是正文区实际
线路长度。

当返回 `XHTTP1_BODY_FIELDS` 时，`TrailerCount` 给出所需描述符数量。调用
`xrtHttp1BodyTrailers` 绑定足够存储后，用同一输入继续解析，不会丢失进度。

close-delimited 模式只有在传输层给出可靠 EOF 时才传入 `bEnd = true`。超时、取消、
RST 和普通 EOF 不能混为同一种上层状态。

## gzip 自动解码

`xrtHttpDecodeCreate` 直接读取解析后的字段数组。把 `xrtHttp1BodyRead` 发布的每个
正文片段交给 `xrtHttpDecodeWrite`，并只在 body reader 完成时设置 `bFinal`。

identity 和显式原样回退路径不复制输入。gzip/deflate 路径按 Content-Encoding
逆序流式解码，验证 gzip Header、CRC、ISIZE 和压缩流终态。每个中间层与最终
输出都受同一明文硬限额约束。

详见 [HTTP 正文解码](http_decode.md)。

## 原始写出

`xrtHttp1RequestWrite` 和 `xrtHttp1ResponseWrite` 不添加 Host、Date、Server 或其他
策略字段。空输出用于精确测量，容量不足不会产生半个 Header。

高性能发送建议：

1. 在连接或协程栈上的小缓冲中写 Header；
2. 用 `xrtNetStreamSendVec` 一次提交 Header 与小正文；
3. 大正文用 `xrtNetStreamSendRef`、`SendRefs`、`SendTake` 或 `SendBuffer`；
4. chunked 正文只用 `xrtHttp1ChunkLineWrite` 生成短前缀，数据本身保持引用发送；
5. `XRT_NET_AGAIN` 时等待 writable/drain，不绕过网络队列硬上限。

这条路径只构造必需的线路字节，不创建请求、响应、字典或正文对象。

## 报文长度

- `xhttp1head.Bytes`：起始行、Header 和最终空行的线路长度；
- `xhttp1bodyplan.Length`：定长正文长度，仅在 `FIXED` 模式有效；
- `xhttp1body.Received`：已经发布的应用正文长度；
- `xhttp1body.WireBytes`：已经消费的正文线路长度；
- `xhttp1message.Wire.Size`：连续输入中第一条完整消息的线路总长度；
- `xhttp1message.BodyBytes`：移除 chunked 元数据后的正文长度。

chunked 和 close-delimited 消息在完成前不存在可提前得知的总线路长度。

## TLS 与 Upgrade

HTTP 不拥有 TLS。`http1_tls` 只是块链适配器，不创建连接或安全策略；证书验证、SNI、会话恢复和
ALPN 仍属于通用 TLS 模块。它按需连续化实际 Header，不增加连接级固定缓冲。

`xrtHttpUpgrade*`、`xrtHttpConnection*` 和 WebSocket 握手函数共同完成 Upgrade
验证。101 后的剩余字节必须原样交给新协议，不能继续送入 HTTP body reader。

## 完整消息便利层

`xrtHttp1RequestMessageParse` 和 `xrtHttp1ResponseMessageParse` 适合已经连续驻留在
内存中的报文、测试和协议网关。它们仍只借用输入，不持有堆对象。

固定长度或 close-delimited 正文可由 `xrtHttp1MessageBodyView` 直接查看；chunked
正文使用 `xrtHttp1MessageBodyCopy` 去除分帧。真正的网络热路径应优先使用流式
body reader，避免等待和复制整个消息。

## 方法与状态

### `xrtHttpMethodParse`

按大小写敏感规则分类 HTTP 方法；合法扩展方法返回 OTHER。

```c
xhttpmethod xrtHttpMethodParse(xstrview Method);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Method` | 输入 | 借用 | 方法文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_METHOD_GET/POST/...` | 已知方法 | — |
| `XHTTP_METHOD_OTHER` | 合法扩展方法 | — |
| `XHTTP_METHOD_INVALID` | 空值或非法 token | 不设置错误 |

#### 错误

- 无 — INVALID 是分类结果

#### 范例

[http/method_tour · 方法](../../examples/http/method_tour/main.c) · 观察

```c
		printf("parse=%u", (unsigned)xrtHttpMethodParse(SV("GET")));
```


### `xrtHttpMethodEqual`

按 HTTP 大小写敏感规则比较两个合法方法名。

```c
bool xrtHttpMethodEqual(
	xstrview Left,
	xstrview Right
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Left` | 输入 | 借用 | 左方法名 |
| `Right` | 输入 | 借用 | 右方法名 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 逐字节相等（HTTP 方法区分大小写） | — |
| `false` | 不等 | 纯比较 |

#### 错误

- 无 — 纯比较

#### 范例

[http/method_tour · 方法](../../examples/http/method_tour/main.c) · 观察

```c
		xrtHttpMethodEqual(SV("PATCH"), SV("PATCH")) ? 1
```


### `xrtHttpMethodSafe`

判断方法是否只读取资源语义；GET、HEAD、OPTIONS 和 TRACE 属于安全方法。

```c
bool xrtHttpMethodSafe(xstrview Method);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Method` | 输入 | 借用 | 方法文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 安全方法 | — |
| `false` | 非安全 | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http/method_tour · 方法](../../examples/http/method_tour/main.c) · 观察

```c
		printf(" safe=%d", xrtHttpMethodSafe(SV("GET")) ? 1
```


### `xrtHttpMethodIdempotent`

判断方法是否允许重复执行而不改变预期效果；安全方法、PUT 和 DELETE 属于幂等方法。

```c
bool xrtHttpMethodIdempotent(xstrview Method);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Method` | 输入 | 借用 | 方法文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 幂等 | — |
| `false` | 非幂等 | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http/method_tour · 方法](../../examples/http/method_tour/main.c) · 观察

```c
		printf(" idem=%d\n", xrtHttpMethodIdempotent(SV("DELETE")) ? 1
```


### `xrtHttpStatusText`

返回已注册状态码的标准原因短语；未知、临时或未分配状态返回空视图。

```c
xstrview xrtHttpStatusText(uint16 iStatus);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iStatus` | 输入 | — | 状态码 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 静态原因短语（人类可读，协议逻辑不得依赖） | — |
| 空视图 | 未注册状态码 | 纯查询 |

#### 错误

- 无 — 查询结果即答案

#### 范例

[http/method_tour · 状态](../../examples/http/method_tour/main.c) · 观察

```c
		xstrview Text = xrtHttpStatusText(200);
```


### `xrtHttpResponseContentAllowed`

判断最终响应是否允许携带内容；HEAD、1xx、204、205、304 和成功 CONNECT 返回假。

```c
bool xrtHttpResponseContentAllowed(
	xstrview Method,
	uint16 iStatus
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Method` | 输入 | 借用、大小写敏感 | 请求方法 |
| `iStatus` | 输入 | 100–999 | 状态码 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 允许携带内容 | — |
| `false` | 禁止内容状态或无效方法/状态 | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http/method_tour · 状态](../../examples/http/method_tour/main.c) · 观察

```c
		printf(" content-allowed=%d\n",
			xrtHttpResponseContentAllowed(SV("HEAD"), 204) ? 1 : 0);
```


## 令牌与 OWS

### `xrtHttpTokenValid`

判断文本是否是非空 HTTP token。

```c
bool xrtHttpTokenValid(xstrview Text);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 待判定文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 非空合法 token | — |
| `false` | 空或含非法字符 | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http/token_tour · 令牌](../../examples/http/token_tour/main.c) · 观察

```c
		printf("valid=%d", xrtHttpTokenValid(SV("gzip")) ? 1
```


### `xrtHttpTokenEqual`

按 ASCII 大小写不敏感规则比较两个 token。

```c
bool xrtHttpTokenEqual(xstrview Left, xstrview Right);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Left` | 输入 | 借用 | 左 token |
| `Right` | 输入 | 借用 | 右 token |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 大小写不敏感相等 | — |
| `false` | 不等 | 纯比较 |

#### 错误

- 无 — 纯比较

#### 范例

[http/token_tour · 令牌](../../examples/http/token_tour/main.c) · 观察

```c
		printf(" eq=%d\n", xrtHttpTokenEqual(SV("GZIP"), SV("gzip")) ? 1
```


### `xrtHttpOwsTrim`

剥离两端的可选空白（SP/HTAB）并返回剩余视图。

```c
xstrview xrtHttpOwsTrim(xstrview Text);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | 输入文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 子视图 | 去除两端 OWS 的借用视图 | 纯切片 |

#### 错误

- 无 — 纯切片

#### 范例

[http/token_tour · OWS](../../examples/http/token_tour/main.c) · 观察

```c
	Trimmed = xrtHttpOwsTrim(SV("  value  "));
```


### `xrtHttpTokenNext`

按 RFC 接收方规则迭代 token-list，忽略逗号空元素；`Offset` 初始为零。

```c
xhttpnext xrtHttpTokenNext(
	xstrview List,
	size_t* pOffset,
	xstrview* pToken
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `List` | 输入 | 借用 | token-list 文本 |
| `pOffset` | 输入/输出 | 非空、初始零 | 游标 |
| `pToken` | 输出 | 非空 | 接收条目（借用输入） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM` | 取得一个 token，游标前进 | — |
| `XHTTP_NEXT_END` | 迭代完成 | 不设错 |
| `XHTTP_NEXT_ERROR` | 非空元素语法错误 | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误 — 非空元素非法 token

#### 范例

[http/base · 字段值迭代](../../examples/http/base/main.c) · 观察

```c
	while ( (Next = xrtHttpTokenNext(
		Field.Value, &iOffset, &Token
	)) == XHTTP_NEXT_ITEM ) {
```


### `xrtHttpTokenListHas`

判断完整 token-list 是否包含指定 token；非空元素语法错误仍返回 false 并设置错误。

```c
bool xrtHttpTokenListHas(xstrview List, xstrview Token);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `List` | 输入 | 借用 | token-list |
| `Token` | 输入 | 借用 | 查找目标（大小写不敏感） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 包含 | — |
| `false` | 不包含，或列表非法 | 非法时设置 `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误 — 非空元素非法

#### 范例

[http/token_tour · 列表](../../examples/http/token_tour/main.c) · 观察

```c
		printf("has=%d", xrtHttpTokenListHas(SV(sList), SV("deflate")) ? 1
```


### `xrtHttpTokenListCount`

统计 token-list 非空条目；空列表成功返回零。

```c
bool xrtHttpTokenListCount(xstrview List, size_t* pCount);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `List` | 输入 | 借用 | token-list |
| `pCount` | 输出 | 非空 | 接收计数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 计数已写出 | — |
| `false` | 非空元素语法错误 | `*pCount` 不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/token_tour · 列表](../../examples/http/token_tour/main.c) · 观察

```c
		if ( xrtHttpTokenListCount(SV(sList), &iCount) ) {
```


### `xrtHttpTokenListWrite`

规范写出逗号空格分隔的 token-list；空输出可精确查询长度。

```c
bool xrtHttpTokenListWrite(
	const xstrview* pTokens,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTokens` | 输入 | 借用数组 | token 视图数组 |
| `iCount` | 输入 | — | 条目数 |
| `pOutput` | 输出 | 可空 | 空+零容量 = 只查长度 |
| `iCapacity` | 输入 | — | 容量 |
| `pSize` | 输出 | 可空 | 实际/所需长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已写出或长度已发布 | — |
| `false` | 参数或容量错误 | 输出不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `XERR_RANGE` — 容量不足（给出所需长度）

#### 范例

[http/token_tour · 列表](../../examples/http/token_tour/main.c) · 观察

```c
		if ( xrtHttpTokenListWrite(Tokens, 2u, Buffer, sizeof(Buffer), &iSize) ) {
```


### `xrtHttpTokenListBuild`

构建零结尾 token-list，返回值由 `xrtFree` 释放。

```c
str xrtHttpTokenListBuild(
	const xstrview* pTokens,
	size_t iCount,
	size_t* pSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTokens` | 输入 | 借用数组 | token 数组 |
| `iCount` | 输入 | — | 条目数 |
| `pSize` | 输出 | 可空 | 接收长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 非空 | 零结尾列表 | — |
| `NULL` | 参数错误或 OOM | 错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `XERR_MEMORY`

#### 范例

[http/token_tour · 列表](../../examples/http/token_tour/main.c) · 观察

```c
		sBuilt = xrtHttpTokenListBuild(Tokens, 3u, NULL);
```


## 权重与长度

### `xrtHttpQualityParse`

严格解析 RFC qvalue（接受两端 OWS），结果范围 0–1000。

```c
bool xrtHttpQualityParse(
	xstrview Text,
	uint16* pQuality
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Text` | 输入 | 借用 | qvalue 文本 |
| `pQuality` | 输出 | 非空、不得与 Text 重叠 | 接收千分值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已写出（0–1000） | — |
| `false` | 语法错误或参数错误 | 输出保持为零/不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或视图非法
- `xrt.http` 域错误 — 非法 qvalue

#### 范例

[http/method_tour · 权重](../../examples/http/method_tour/main.c) · 观察

```c
		if ( !xrtHttpQualityParse(SV("0.5"), &iQuality) ) {
```


### `xrtHttpWeightedTokenNext`

迭代 token [ weight ] 列表并忽略空成员；缺省 Quality 为 1000。可直接用于 Accept-Encoding 等字段。

```c
xhttpnext xrtHttpWeightedTokenNext(
	xstrview List,
	size_t* pOffset,
	xhttpweightedtoken* pItem
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `List` | 输入 | 借用 | 加权列表文本 |
| `pOffset` | 输入/输出 | 初始零 | 游标 |
| `pItem` | 输出 | 非空 | 接收 Token+Quality |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_NEXT_ITEM` | 取得一项 | — |
| `XHTTP_NEXT_END` | 完成 | 不设错 |
| `XHTTP_NEXT_ERROR` | 语法错误 | `xrt.http` 域错误 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/token_tour · 加权迭代](../../examples/http/token_tour/main.c) · 观察

```c
		while ( xrtHttpWeightedTokenNext(SV(sWeighted), &iOffset,
			&Item) == XHTTP_NEXT_ITEM ) {
```


### `xrtHttpContentLengthParse`

解析 Content-Length；逗号分隔的重复值只有完全一致时才成功。

```c
bool xrtHttpContentLengthParse(
	xstrview Value,
	uint64* pLength
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Value` | 输入 | 借用 | 字段值 |
| `pLength` | 输出 | 非空 | 接收长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 长度已写出 | — |
| `false` | 非法/重复不一致 | 输出保持为零 |

#### 错误

- `xrt.http` 域错误 — 非数字或重复值不一致

#### 范例

[http/method_tour · 长度](../../examples/http/method_tour/main.c) · 观察

```c
		if ( !xrtHttpContentLengthParse(SV("42"), &iLength) ) {
```


## Host 与 Authority

### `xrtHttpHostParse`

解析单个 Host 字段值为借用 authority 结构；空字段值与空端口按 RFC 保留，PORT_VALUE 表示可用网络数值。

```c
bool xrtHttpHostParse(
	xstrview Value,
	xhttpauthority* pHost
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Value` | 输入 | 借用、不含 OWS/字段名 | 字段值 |
| `pHost` | 输出 | 非空、支持未对齐存储 | 接收借用视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 结构已发布（视图借用输入） | — |
| `false` | 语法错误 | 输出不变 |

#### 错误

- `xrt.http` 域错误 — 非法 authority

#### 范例

[http/host · 解析](../../examples/http/host/main.c) · 观察

```c
	if ( !xrtHttpHostParse(
		XRT_STR_LITERAL("[2001:db8::1]:8443"), &Host
	) || !xrtHttpAuthorityPort(&Host, 80u, &iPort) ) {
```


### `xrtHttpHostValid`

验证 Host 字段值是单个、无 userinfo 的 URI authority。

```c
bool xrtHttpHostValid(xstrview Value);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Value` | 输入 | 借用 | 字段值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 合法 | — |
| `false` | 非法 | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http/validate_tour · Host](../../examples/http/validate_tour/main.c) · 观察

```c
		xrtHttpHostValid(SV("example.com")) ? 1
```


### `xrtHttpIpv4Valid`

严格验证 RFC 3986 IPv4 文本；拒绝多段、越界值和前导零。

```c
bool xrtHttpIpv4Valid(xstrview Value);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Value` | 输入 | 借用 | IPv4 文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 合法 | — |
| `false` | 非法（含 `01.2.3.4` 前导零） | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http/validate_tour · IPv4](../../examples/http/validate_tour/main.c) · 观察

```c
		xrtHttpIpv4Valid(SV("01.2.3.4")) ? 1
```


### `xrtHttpIpv6Valid`

严格验证 IPv6 文本，支持压缩和嵌入式 IPv4，不接受 ZoneID。

```c
bool xrtHttpIpv6Valid(xstrview Value);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Value` | 输入 | 借用 | IPv6 文本 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 合法 | — |
| `false` | 非法 | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http/validate_tour · IPv6](../../examples/http/validate_tour/main.c) · 观察

```c
		xrtHttpIpv6Valid(SV("
```


### `xrtHttpHostEqual`

按 ASCII 大小写不敏感规则比较两个已拆分 Host 视图。

```c
bool xrtHttpHostEqual(xstrview Left, xstrview Right);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Left` | 输入 | 借用 | 左 Host |
| `Right` | 输入 | 借用 | 右 Host |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 相等（host 大小写不敏感） | — |
| `false` | 不等 | 纯比较 |

#### 错误

- 无 — 纯比较

#### 范例

[http/validate_tour · Host](../../examples/http/validate_tour/main.c) · 观察

```c
		xrtHttpHostEqual(SV("EXAMPLE.com"), SV("example.com")) ? 1
```


### `xrtHttpAuthorityValid`

验证拆分后的 authority 字段、标志与端口数值保持一致。

```c
bool xrtHttpAuthorityValid(
	const xhttpauthority* pAuthority
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAuthority` | 输入 | 非空 | 待验证结构 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 一致 | — |
| `false` | 不一致 | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[http/validate_tour · Authority](../../examples/http/validate_tour/main.c) · 观察

```c
		xrtHttpAuthorityValid(&Auth) ? 1
```


### `xrtHttpAuthorityPort`

取得显式端口；省略或空端口使用调用方给出的默认值。

```c
bool xrtHttpAuthorityPort(
	const xhttpauthority* pAuthority,
	uint16 iDefaultPort,
	uint16* pPort
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAuthority` | 输入 | 非空 | authority |
| `iDefaultPort` | 输入 | — | 默认端口 |
| `pPort` | 输出 | 非空 | 接收端口 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 端口已写出 | — |
| `false` | 端口越界或参数错误 | 输出不变 |

#### 错误

- `xrt.http` 域错误 — 端口超出 `uint16`

#### 范例

[http/host · 端口](../../examples/http/host/main.c) · 观察

```c
	) || !xrtHttpAuthorityPort(&Host, 80u, &iPort) ) {
```


### `xrtHttpTargetParse`

按方法严格解析 request-target；CONNECT 只接受带非空端口 authority，OPTIONS 星号须精确为 "*"。

```c
bool xrtHttpTargetParse(
	xstrview Method,
	xstrview Text,
	xhttptarget* pTarget
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Method` | 输入 | 借用 | 方法文本 |
| `Text` | 输入 | 借用 | target 文本 |
| `pTarget` | 输出 | 支持未对齐存储、不得覆盖输入 | 接收解析结果 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已解析 | — |
| `false` | 语法错误 | 输出不变 |

#### 错误

- `xrt.http` 域错误 — 非法 target

#### 范例

[http/target · 解析](../../examples/http/target/main.c) · 观察

```c
	if ( !xrtHttpTargetParse(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL(
			"https://example.test:8443/items?q=1"
```


## 编码枚举

### `xrtHttpTargetAuthority`

解析请求的有效 authority：absolute/CONNECT 用 target，origin/星号用 Host 字段值。

```c
bool xrtHttpTargetAuthority(
	const xhttptarget* pTarget,
	xstrview Host,
	xhttpauthority* pAuthority
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pTarget` | 输入 | 非空 | 已解析 target |
| `Host` | 输入 | 借用 | Host 字段值 |
| `pAuthority` | 输出 | 未对齐存储、不得覆盖输入 | 接收 authority |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 已写出 | — |
| `false` | 无可用 authority 或非法 | 输出不变 |

#### 错误

- `xrt.http` 域错误

#### 范例

[http/target · authority](../../examples/http/target/main.c) · 观察

```c
	) || !xrtHttpTargetAuthority(
		&Target,
		XRT_STR_LITERAL("ignored.test"),
```


### `xrtHttpCodingParse`

把编码 token 解析为内置枚举（identity/gzip/deflate，x-gzip 为 gzip 别名）；未知返回 NONE。

```c
xhttpcoding xrtHttpCodingParse(xstrview Token);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Token` | 输入 | 借用 | 编码 token（大小写不敏感） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `XHTTP_CODING_IDENTITY/GZIP/DEFLATE` | 内置编码 | — |
| `XHTTP_CODING_NONE` | 未知编码（如 zstd） | 不设错 |

#### 错误

- 无 — NONE 是分类结果

#### 范例

[http/small_fields · 编码枚举](../../examples/http/small_fields/main.c) · 观察

```c
		if ( (xrtHttpCodingParse(SV("gzip")) !=
				XHTTP_CODING_GZIP) ||
```



## 扩展库边界

客户端池、重定向、重试、缓存、认证、Cookie、MIME、Multipart、FormData、SSE、
服务器路由和中间件属于独立发布的 `xhttp` 扩展；它们不参与 XRT 核心构建、单头包或
兼容性承诺。
