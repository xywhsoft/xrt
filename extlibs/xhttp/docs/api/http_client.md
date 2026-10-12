# HTTP 客户端

## 请求构建器

`http_client_request` 提供客户端出站请求的拥有型构建器。它只依赖 URL、
Header 和正文源，不创建网络连接；客户端执行、连接池、重定向和解压属于后续
独立裁剪层。

```c
xhttprequest* request = xrtHttpRequestCreate(
	XRT_STR_LITERAL("POST"),
	XRT_STR_LITERAL("https://example.test/api")
);

xrtHttpRequestSetBytes(
	request,
	(xbytesview){ (const uint8*)json, json_size },
	XRT_STR_LITERAL("application/json; charset=utf-8")
);
```

请求对象拥有方法、原始 URL 和全部 Header。正文通过 `xhttpbody` 引用共享，
因此 Clone 不复制正文数据。

默认构造使用 `xrtHttpHeadersConfigInit()` 的安全上限。需要更大字段或更严格
应用限额时，使用 `xrtHttpRequestCreateWithHeaders()` 传入 Header 配置；请求
层没有额外固定字段长度。

## URL 契约

客户端请求只接受带 authority 和非空 host 的绝对 `http` 或 `https` URL：

- fragment 可以保留在原始 URL 中，但 origin-form target 不发送 fragment。
- 空 host 被拒绝；空端口作为 URI 词法事实保留，执行时按 scheme 使用默认端口。
- URI 基础层允许任意长度端口文本；客户端请求要求最终端口位于 `1..65535`。
	超范围端口使用 `XHTTP_REQUEST_ERROR_URL`，Cause 保留 URL 层的 `XERR_RANGE`。
- URL userinfo 被拒绝，认证应通过明确的 Authorization API 或 Header 设置。
- 方法必须是非空 HTTP token。

空值、畸形 URL 和不支持的 scheme 使用 `http.request` 域与
`XHTTP_REQUEST_ERROR_URL`、`XHTTP_REQUEST_ERROR_SCHEME` 等稳定代码；底层 URL
解析错误保留为 Cause。`Data == NULL && Size != 0` 的非法借用视图属于参数错误，
不会被伪装成协议值错误。全部失败路径保持原请求不变。

`xrtHttpRequestUrl()` 返回借用的解析结果，`xrtHttpRequestUrlText()` 返回借用的
原始文本。修改 URL 会使旧视图失效。

## 抓取与资源发现边界

HTTP Client 负责 HTTP 传输与协议状态，包括 DNS、连接池、代理、TLS、Cookie、
重定向、正文分帧、解压、截止时间、取消、背压和资源上限。它不把网页抓取策略塞进
连接对象：HTML 解析、`robots.txt`、站点范围、每 Origin 礼貌延迟、抓取前沿、内容
落盘和去重键都属于可独立替换的应用层。

构建抓取器时应直接复用这些公开层次：

- URL 解析和相对链接解析使用 `xrtUrlParse`、`xrtUrlResolve`；HTTP 同源判断使用
  `xrtHttpOriginFromUrl` 和 `xrtHttpOriginSame`。
- 请求执行使用 Client/Call API，不得另写阻塞 socket、固定大小响应缓冲或手工
  `Content-Length`/chunked 解析器。
- Redirect、正文大小、Header 大小、解压后大小、并发数和等待队列都必须设置硬上限；
  取消应传入当前 Call，而不是只设置一个最终才会观察到的全局标志。
- HTML 是独立且复杂的语法。只搜索 `href=\"` 或 `src=\"` 会错误读取注释、脚本和
  文本，也无法处理实体、引用形式与 `<base>`；xrt 不把这种扫描器包装成标准库 API。

这种边界保留了完整的底层扩展能力，同时避免 HTTP Client 与特定爬虫、DOM 或存储
模型耦合。

## Header

`AddHeader` 保留同名字段，`SetHeader` 设置首项并删除其他同名项，
`RemoveHeader` 删除全部同名字段。底层复用唯一的 `xhttpheaders` 实现，不维护
第二套 Header 存储或字段长度限制。

客户端执行层会负责 Host、Content-Length 和 Transfer-Encoding 的安全分帧。
请求构建器允许保存这些字段，但执行时必须校验用户字段与实际传输计划一致。

## 响应 Trailer 声明

启用 `http_client_request_te` 后，`xrtHttp1RequestAcceptTrailers()` 一步声明客户端
愿意接收 HTTP/1 响应 Trailer：

```c
xrtHttp1RequestAcceptTrailers(request);
```

该函数先严格验证全部重复 `TE` 与 `Connection` 字段，再按需追加
`TE: trailers` 和 `Connection: TE`。已有传输编码、权重、连接选项和字段顺序均被
保留；重复调用不增加字段或分配内存。修改在独立 Header 深副本中完成，只有两项都
成功后才交换到请求，因此字段限额、内存不足或既有字段非法都不会留下半套状态。

名称中的 `Http1` 明确表示 `Connection` 是 HTTP/1 逐跳字段。将来 HTTP/2 或 HTTP/3
执行层不应把这组线路字段直接编码到对应协议；其 Trailer 能力由各自协议适配层表达。
需要完全自定义时，仍可用普通 Header API 直接设置字段。

## 请求 Trailer

启用 `http_client_request_trailers` 后，请求构建器提供与 Header 对称的拥有型
Trailer 容器：

```c
xrtHttpRequestAddTrailer(
	request,
	XRT_STR_LITERAL("Digest"),
	XRT_STR_LITERAL("sha-256=:precomputed:")
);
```

`AddTrailer`、`SetTrailer`、`RemoveTrailer`、`Trailer`、`TrailerCount`、
`TrailerData` 和 `TrailerAt` 提供常用操作。`TrailerData` 返回连续只读数组，
`xrtHttpRequestTrailers()` 返回只读容器且不会触发
分配；`xrtHttpRequestEditTrailers()` 在需要直接使用通用 Header API 时才创建
容器。未使用 Trailer 的普通请求不增加动态分配，Clone 对 Trailer 做深拷贝。

字段值必须在 `xrtHttp1RequestPrepare()` 前定稿。准备层验证每个字段能否作为
Trailer，强制使用 HTTP/1.1 chunked 分帧，并根据实际字段名重新生成唯一
`Trailer` 声明；调用方可以省略声明，重复的显式声明也会被整体替换，不会成为第二份
真相。禁止字段、只有声明而没有实际字段、`Content-Length` 和非 chunked
`Transfer-Encoding` 都在网络操作前失败。空正文也可以携带 Trailer，此时线路
只发送 last-chunk 与字段。

## 正文

- `xrtHttpRequestSetBody()` 保留一个正文引用。
- `xrtHttpRequestSetBytes()` 复制字节，并可在同一次失败原子操作中设置
  `Content-Type`。
- 两个接口都不提前写入 Content-Length；已知长度和 chunked 选择由执行层决定。
- 清除正文不会隐式删除用户设置的 Header。

请求数据便利层直接适配公开协议容器，不在客户端内部复制 URL、Query、Form 或
Multipart 编码逻辑：

```c
xqueryparams* query = xrtQueryParamsCreate(NULL);
xformdata* form = xrtFormDataCreate(NULL);
xmultipartboundary boundary;

xrtQueryParamsAppend(
	query,
	XRT_STR_LITERAL("q"),
	XRT_STR_LITERAL("xrt runtime")
);
xrtHttpRequestSetQueryParams(request, query);

xrtFormDataAppendText(
	form,
	XRT_STR_LITERAL("title"),
	XRT_STR_LITERAL("demo")
);
xrtHttpRequestSetFormDataRandom(request, form, &boundary);
```

- `xrtHttpRequestSetQueryParams()` 只替换 URL 查询组件，保留 path 与 fragment；
  空容器生成显式空查询 `?`。
- `xrtHttpRequestSetForm()` 把 `xqueryparams` 编码为
  `application/x-www-form-urlencoded` 固定正文；编码存储直接交给正文对象，
  不进行第二次正文复制。
- `xrtHttpRequestSetFormData()` 使用调用方给定的已校验 boundary，把 FormData
  组合成可流式、能力可传播的 `xhttpbody`。
- `xrtHttpRequestSetFormDataRandom()` 生成安全随机 boundary；只有正文和
  Content-Type 全部提交成功后才写入输出结构。

四层分别由 `http_client_request_query`、`http_client_request_form`、
`http_client_request_form_data` 和 `http_client_request_form_data_random` 控制，
不用表单的客户端不会带入 QueryParams、Multipart、随机数或正文组合器。所有操作
都失败原子：URL 重建、正文组合、Header 限额或内存分配失败时，请求保留原 URL、
正文与 Content-Type。错误统一提升到 `http.request` 域，并分别使用
`XHTTP_REQUEST_ERROR_QUERY`、`XHTTP_REQUEST_ERROR_FORM` 和
`XHTTP_REQUEST_ERROR_FORM_DATA`，底层 URL、QueryParams、FormData、Multipart
或内存错误保留为 Cause。

## 生命周期

请求构建器允许修改，但不允许并发修改或边修改边 Clone。异步执行入口必须取得
独立快照，因此调用方可以在提交后立即修改或销毁原请求。`Clone` 的方法、
URL、Header 和 Trailer 完全独立，正文只增加引用。
Clone 同时保留源 Header/Trailer 容器的容量策略和资源限额，不会退回默认配置。

## 响应结果

`http_client_response` 提供客户端完成后交给调用方的只读 `xhttpresponse`：

- 版本、状态码、reason phrase、Header、trailer 和最终有效 URL 都由响应拥有。
- 缓冲执行通过 `xrtHttpResponseBody()` 返回连续正文。
- 流式执行不分配正文缓冲，只由 `xrtHttpResponseBodyBytes()` 记录已交付字节。
- `xrtHttpResponseWireBodyBytes()` 单独记录解压前的编码正文载荷。
- `xrtHttpResponseBodyText()` 复制正文并附加零字符，不会把内部二进制零字节当成结尾。

空响应不会预留固定 8 KiB 或其他传输缓冲。缓冲正文只在收到数据后按需增长；
首次分配按实际数据量完成，后续才倍增；真正的流式消费路径不因响应对象产生
正文内存增长。

`http_client_content_type` 与 `http_client_set_cookie` 是两个独立可裁剪的响应便利层：

```c
xmediatype type;
size_t header = 0;
xsetcookie cookie;

if ( xrtHttpResponseContentType(response, &type) == XHTTP_NEXT_ITEM ) {
	/* type 借用响应拥有的 Content-Type 字段。 */
}

while ( xrtHttpResponseSetCookieNext(
	response, &header, &cookie
) == XHTTP_NEXT_ITEM ) {
	/* 每个 cookie 独立借用一条 Set-Cookie 字段。 */
}
```

`Content-Type` 缺失返回 `END`，唯一合法字段返回 `ITEM`，重复或语法错误返回
`ERROR`。`Set-Cookie` 迭代器按 Header 线路顺序扫描，不按逗号拆分或合并；成功后
索引指向下一字段，结束时等于 Header 数量。单条解析失败时索引停在错误字段，
调用方可以检查该原始 Header 后递增索引继续。两组解析结果都只在响应销毁前有效，
且不产生额外分配。

响应便利层错误使用 `xrt.http.client.response` 域。参数、越界索引、重复单值 Header、
非法媒体类型和非法 Set-Cookie 分别使用 `XHTTP_RESPONSE_ERROR_ARGUMENT`、
`INDEX`、`HEADER`、`CONTENT_TYPE` 与 `SET_COOKIE`；底层解析错误保留为 Cause。
不需要结构化读取时仍可直接使用 `xrtHttpResponseHeaderAt()` 与
`xrtHttpResponseHeaders()`，这两个便利模块不是客户端执行的强制依赖。
`http_client_response_data_oom_tests` 会逐次失败响应、Header 和结构化错误的每个
逻辑分配点，要求响应存储、Cause 链和线程错误在每轮后全部回到空基线。

## HTTP/1 请求准备

`http_client_prepare` 把可变 `xhttprequest` 冻结为不可变
`xhttp1requestplan`。计划拥有方法、完整原始 URL、HTTP/1.1 Header、可选
last-chunk、request-target 和连接端点 host，并保留独立正文引用；创建成功后
可以立即修改或销毁原请求。

```c
xhttp1requestplan* plan = xrtHttp1RequestPrepare(request, NULL);
xbytesview head = xrtHttp1RequestPlanHead(plan);
xbytesview end = xrtHttp1RequestPlanEnd(plan);

/* head 和 chunked end 可直接借出，正文通过 PlanBody 打开 Reader。 */
```

默认使用 auto：`CONNECT` 自动生成带端口的 authority-form，其余方法自动使用
origin-form。`xhttp1requestoptions` 还可显式选择 origin-form、absolute-form、
authority-form、asterisk-form 和严格校验的自定义 target，覆盖正向代理、
CONNECT、`OPTIONS *` 与自定义传输；方法与目标形式必须符合 HTTP 语义，
fragment 在自动生成路径中永远不会上线。

准备阶段统一执行下列安全契约：

- HTTP/1.1 缺失 `Host` 时从 URL 生成，显式 `Host` 必须是单个合法 authority；
- absolute-form 的显式 `Host` 必须与 URL 使用相同的大小写不敏感 host 和有效
  端口；省略端口、空端口和显式默认端口按同一 origin 比较。origin-form 仍允许
  覆盖 `Host` 以选择虚拟主机；
- 重复 `Host`、`Content-Length` 或 `Transfer-Encoding` 被拒绝；
- `Content-Length` 必须与正文声明长度一致，且不能和
  `Transfer-Encoding` 共存；
- 未知长度正文自动选择 `chunked`，已知长度正文自动生成
  `Content-Length`，请求对象本身不被修改；
- 启用请求 Trailer 层时，实际字段强制选择 chunked，并生成规范 `Trailer`
  声明和冻结的 last-chunk；未启用时显式 `Trailer` 会在发送前失败；
- `TRACE` 正文和畸形 `Connection` 会在网络操作前失败；重复或列表形式的
  `Expect: 100-continue` 统一发布一个 Continue 事实，语法正确的扩展 expectation
  因客户端无法执行而失败，畸形 `Expect` 则保留协议解析错误原因链。
- 重复 `TE` 字段按列表组合规则解析；畸形值在网络前失败，存在 `TE` 时必须同时在
  `Connection` 中声明 `TE`，避免逐跳能力被错误转发。
- 2xx CONNECT 响应忽略 `Content-Length` 和 `Transfer-Encoding`，Header
  结束后的字节直接归隧道所有。

HTTP/1.1 本身默认保持连接，准备层不会冗余添加
`Connection: keep-alive`。`xrtHttp1RequestPlanClose` 与
`xrtHttp1RequestPlanExpectContinue` 已公开发布对应事实，供事务状态机正确决定
连接复用和正文发送时机。完整 Header 是二进制视图，不附加零字符，也不能
当作 C 字符串读取。

## 网络执行层

运行时、TLS/Future 和其他网络便利功能已迁入 [xhttp-client](../../../xhttp-client/docs/api/http_client.md)。完整迁移与所有权边界见 [分层说明](../design/http-layers.md)。
