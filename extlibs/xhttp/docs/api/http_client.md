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

## HTTP/1 Stream 调用

`http_client_stream` 在一条已经打开的 TCP Stream 上执行一个
`xhttp1exchange`。启用 `http_client_tls` 后，同一个调用模型也接受已经完成
握手的 TLS Stream：

```c
xhttp1callevents events;

xrtHttp1CallEventsInit(&events);
events.Done = on_done;
events.Progress = on_progress;
events.Data = context;

xhttp1call* call = xrtHttp1CallTcp(
	stream,
	exchange,
	NULL,
	&events
);
```

构造函数必须在 Stream 所属 Worker 上调用。成功后接管调用方的 Stream 引用和
Exchange；失败时两个输入仍归调用方。完成回调也在所属 Worker 上同步执行、
至多一次，并保证不会早于构造函数成功返回。

结果所有权规则：

- 成功且连接可复用时，`Response` 和 `Tcp` 或 `Tls` 的调用方引用转移给回调。
- `101` 或成功的 CONNECT 会设置 `Upgraded`，并把协议切换后的传输一并移交。
- `Buffered` 是 Stream 中尚未被 HTTP 接受的字节数；升级处理器可以在完成
  回调中立即用 Stream 事件接管 API 安装下一协议。
- 不可复用、失败或取消的传输由调用驱动器关闭并释放，不交给回调。
- 回调之外需要保存调用对象时，使用 `xrtHttp1CallRef` 与
  `xrtHttp1CallDestroy` 管理独立引用。

`WriteSize` 默认是 16 KiB，只限制一次 Exchange 借出和传输发送量，不为每个
调用分配固定缓冲。TCP 发送还服从 Stream 的硬队列预算；队列恢复可写后由
Stream 事件继续驱动。

`xrtHttp1CallConfigInit` 和 `xrtHttp1CallEventsInit` 支持未对齐的完整结构存储。
`xrtHttp1CallTcp`、`xrtHttp1CallTls` 在接管传输和 Exchange 前验证地址范围并复制
配置与事件表；回绕地址、空 `Done` 或零 `WriteSize` 都同步失败，输入所有权不变。

`xrtHttp1CallCancel()` 可以从任意线程调用。它与成功、失败终态以及 Stream
所有权转移共享同一个线性化边界：返回 `true` 表示取消已经获胜，完成回调和
`xrtHttp1CallState()` 最终必须报告 `CANCELLED`；返回 `false` 表示已有取消请求
或某个终态已经先提交。取消路径在边界内保留一次临时 Stream 引用，在锁外执行
`Abort`，因此不会持锁进入 TCP、TLS 或用户回调。该边界使用内部短临界区锁，
不把 96 字节公开 Mutex 嵌入每个调用对象。若协议或传输失败与已接纳取消同时
发生，最外层错误仍为 `XERR_CANCELLED`，先到的失败只作为 Cause 保留。
该契约对 TCP 和 TLS 完全一致；TLS 取消会保留临时组合 Stream 引用，并通过
TLS 自身的异步 Abort 路径回收会话与底层 TCP，不向完成回调泄漏半关闭传输。
任意终态提交都会同时清除暂停和待恢复门，因此终态调用的
`xrtHttp1CallPaused()` 始终返回 `false`，已经排队的恢复命令只负责释放自身引用。

可选 `Progress` 回调只报告底层已经接受的请求输出、已经被 HTTP 消费的响应
输入，以及请求完整发送三个事实。`WRITE` 和 `READ` 的 `Bytes` 必须非零，
`REQUEST_DONE` 的 `Bytes` 为零且至多出现一次；排队、重试或只生成待发送视图
不会被误报为进度。回调与 `Done` 在同一 Stream Worker 上执行，适合实现空闲
超时、吞吐统计和分阶段跟踪；可以在 `Progress` 内调用
`xrtHttp1CallCancel()`，取消动作会延迟退出当前驱动栈。不需要观察时留空，
没有额外热路径回调。

## 异步请求正文

基础 `http_client_stream` 不依赖 Future，只接受不会返回 `AGAIN` 的正文来源。
启用独立的 `http_client_stream_async` 后，调用驱动器自动消费
`xrtHttp1ExchangeOutputWait()` 返回的可读性 Future：

- Future 成功完成后，驱动器自动回到 Stream 所属 Worker 继续输出。
- Future 失败、取消或无值关闭时，调用以保留原因为 Cause 的结构化错误结束。
- 调用取消或提前结束时，会请求取消 Future 并摘除 waiter；调用对象的释放
  不依赖正文生产者最终完成 Future。
- Future 完成回调只登记通知；waiter 的 Release 确认回调已经退出后，才提交
  专用的 Worker 内部命令。连续或无数据唤醒不会在旧 waiter 仍处于 `Calling`
  时重用节点，也不依赖公开 Post 队列、临时分配或正文生产线程。
- 服务器可以在请求正文尚未完成时返回最终响应。驱动器优先交付该响应，停止
  正文来源且不发布 `REQUEST_DONE`；这类连接不进入复用池。
- Future 可以在 waiter 注册前完成，立即完成竞态与普通异步完成使用同一契约。

调用方只负责推进正文来源状态并完成对应 Future，不需要额外的手动 Resume
入口。正文 `Next` 在 Future 完成前不会被重复调用。

## 高层 Client

`http_client` 把请求快照、异步 DNS、Happy Eyeballs TCP 拨号、两类截止时间、
取消、诊断和 HTTP/1 Stream 调用组合为一个稳定入口：

```c
xhttpclientconfig Config;
xhttpcalloptions Options;

xrtHttpClientConfigInit(&Config);
xrtHttpCallOptionsInit(&Options);

client = xrtHttpClientCreate(engine, &Config);
call = xrtHttpClientDo(client, request, &Options, on_done, context);
```

启用 `http_client_cache` 并为 `xhttpclientconfig.Cache.Store` 提供
`xhttpcache` 后，高层 Client 会自动完成查找、命中交付、新鲜度判断、条件验证、
存储、部分响应组合和 unsafe 方法失效。存储对象仍保持公开，应用、代理和自定义
持久化后端可以直接使用同一套 [`http_cache_store.md`](http_cache_store.md)
契约。

Engine 必须已经启动。Client 保持 Engine 生命周期，并拥有私有异步 Resolver；
需要跨 Client 共享解析缓存时使用 `xrtHttpClientCreateWithResolver`，该形式借用
Resolver。`xrtHttpClientDo` 在返回成功前复制请求、调用选项中的值语义配置和
分区键，并保留正文引用；调用方可立即修改或销毁原请求。

`xrtHttpClientConfigInit()`、`xrtHttpCallOptionsInit()` 以及各子配置初始化器只要求
目标是足够大的有效连续内存，不要求自然对齐。Client 创建和 Call 提交同样通过
`memcpy` 取得对齐的局部快照；无效或地址计算回绕的配置范围会在读取前以
`XHTTP_CLIENT_ERROR_ARGUMENT` 同步拒绝，且不会发布完成回调。

Client 创建会立即验证 Dial、Stream、Exchange 和私有 Resolver 的静态配置，
无效策略不会延迟到首个异步调用。下层配置错误保留为 Cause，公开错误统一为
`xrt.http.client / XHTTP_CLIENT_ERROR_CONFIG`。

`Timeout` 从提交时起覆盖排队、DNS、TCP、代理、TLS、发送和接收的总时长。
`IdleTimeout` 从 Call 在所选 Worker 开始执行时起，限制没有实际进展的连续
时长；传输可用、底层接受请求字节和 HTTP 消费响应字节都会刷新它。连接池
等待、DNS、连接、代理或 TLS 阶段长时间没有完成也属于无进展。调用选项中的
两个零值分别继承 Client，`XHTTP_CLIENT_TIMEOUT_NONE` 显式关闭对应限制。
提交时已经取消的令牌会在安装监听器、Timer 或启动 DNS/TCP 前直接进入取消
终态；提交后发生的取消由同一个终态门线性化。

总 deadline 不晚于 idle deadline 时不会分配第二条 Timer；两者同刻到达时
稳定报告 `XHTTP_CLIENT_ERROR_TIMEOUT_TOTAL`，否则空闲超时报告
`XHTTP_CLIENT_ERROR_TIMEOUT_IDLE`。刷新 idle deadline 只写原子时间值，不会
为每次收发分配 Timer 或投递命令。`Cancel` 在提交时增加引用；
`xrtHttpCallCancel` 可从任意线程调用。

`xhttpcalloptions.ResponseBodyLimit` 为零时继承
`xhttpclientconfig.Exchange.Body.MaxBody`，其他值只覆盖本次 Call；`UINT64_MAX`
显式允许无界流式响应。限额作用于移除 HTTP/1 分帧后、执行 `Content-Encoding`
解码前的表示正文；网络响应、缓存命中和本地生成的 Range/multipart 缓存响应使用同一
预算，即使使用 Body 回调而不建立完整响应缓冲也会拒绝超限输入。自动解码后的明文
另由 `xhttpclientconfig.Decompress.MaxBody` 限制，防止高压缩比内容绕过内存预算。
SSE Client 会明确选择无界表示正文，再依靠 Parser 的单行、事件数据、类型和 ID
限额约束结构化内存，避免把长连接误当成有限下载。

`xrtHttpCallInfo` 可从任意线程复制运行中或终态快照，完成回调的
`xhttpcallresult.Info` 携带同一终态：

- `State` 表示排队、执行或不可变终态，`Phase` 在终态后仍保留实际结束阶段。
- `Completed` 非零表示终态已经发布；此后 `State`、`Result`、`Error` 和其他
  终态字段全部冻结，较晚到达的取消、超时或传输失败不能改写已发布结果。
- `Error` 是高层 Client 分类；成功为 `XHTTP_CLIENT_ERROR_NONE`，失败、取消
  和超时与完成回调观察到的 `Result` 保持一致。
- 时间均来自单调时钟、单位为微秒，未到达的时间点为零。
- `RequestWireBytes` 与 `ResponseWireBytes` 累计整个重定向链的线上字节。
- `ResponseBodyBytes` 是最终可见响应交付的正文大小；自动解压时记录明文，
  流式消费也会计数。
- `TransportReady`、`RequestSent` 和 `FirstByte` 保留完整调用中首次到达的
  时间；`Headers` 是重定向过滤后最终可见 Header 的首次到达时间。
- `ReusedConnection` 表示任意一跳曾复用连接，`Secure` 描述当前或最终一跳，
  `Redirects` 是实际已经跟随的跳数；启用重试层时，`Retries` 是退避结束后
  真正开始的后续尝试数，不包含只做出决定但没有启动的尝试。

`xhttpcalloptions.Events` 是高层 Call 事件，不暴露底层 Exchange。Informational、
Headers 和 Body 回调都直接携带当前 `pCall`，网络响应与缓存响应保持相同签名：

```c
static bool on_body(
	xhttpcall* pCall,
	const xhttpresponse* pResponse,
	xbytesview Data,
	ptr pData
)
{
	(void)pResponse;
	return consume_body(pData, Data) || xrtHttpCallPause(pCall);
}

Options.Events.Body = on_body;
Options.Events.Data = context;
```

所有事件与完成回调都在所选网络 Worker 上同步执行，并可能与提交线程并发。
必须使用回调参数中的 `pCall`，不能假设保存 `xrtHttpClientDo()` 返回值的赋值已经
发生。`Events.Data` 必须覆盖 Call 生命周期；响应和正文视图只借用到当前回调返回。

`xrtHttpCallWorker` 返回 Call 从提交到终态始终所属的借用 Worker，供需要把恢复或
自定义状态投递回同一执行域的组合层使用。`xrtHttpCallRequestClone` 可以从任意线程
克隆当前有效请求；没有发生重定向时是初始冻结请求，执行中是当前一跳请求，调用
结束后是最终一跳请求。
方法、URL、Header 和 Trailer 深复制，正文只增加引用，调用方负责
`xrtHttpRequestDestroy`。克隆与重定向替换由同一 Call 锁线性化，失败统一使用
`xrt.http.client / XHTTP_CLIENT_ERROR_REQUEST`，底层请求或内存错误保留为 Cause。

同步提交失败返回空指针、不调用完成回调，也不接管请求；Request 冻结、HTTP/1
计划或响应 Exchange 构造失败同样提升到 `xrt.http.client`，原始协议、配置或
内存错误保留为 Cause。提交成功后，调用方拥有一个 Call 引用；运行时另持有
内部引用直到终态。回调中的 `Response` 和升级后的传输引用转移给调用方，
`Buffered` 只报告 Upgrade 后仍留在该传输中的协议外字节，普通响应始终为零；
`Error` 只借用 Call。跨回调保留 Call 时使用 `xrtHttpCallRef`，最后用
`xrtHttpCallDestroy` 释放。

HTTP 完成表示响应或 Upgrade 已经形成终态，不等待未交付连接的对端关闭确认。
普通 TCP FIN 或 TLS `close_notify` 的异步清理可能短暂保留 Engine 对象和关闭
Timer；Engine 的活动对象统计归零后才能停止或销毁。连接池接管的可复用传输
则由 Client 池策略管理，直到归还、过期或显式关闭空闲连接。

Client 自身使用与 Server 对称的单向生命周期：`RUNNING -> DRAINING/ABORTING ->
CLOSED`。`xrtHttpClientDrain()` 原子停止新 Call，关闭空闲连接，并让已经提交的
排队、DNS、拨号、TLS 和 HTTP Call 自然完成；`xrtHttpClientAbort()` 可以从
`RUNNING` 或 `DRAINING` 升级为异常终止，并协作取消所有尚未提交终态的 Call。
两个入口都可从任意线程调用且幂等；排空后再 Abort 允许，Abort 后不能降回 Drain。
`xrtHttpClientState()` 提供并发状态快照。`xrtHttpClientEngine()` 返回 Client 创建时绑定的借用网络 Engine，供同一 Worker/Engine 上的协议适配层判断调度上下文；返回值不转移所有权，生命周期不得超过 Client。

- `XHTTP_CLIENT_RUNNING`：接受新 Call。
- `XHTTP_CLIENT_DRAINING`：拒绝新 Call，等待已提交 Call 自然完成。
- `XHTTP_CLIENT_ABORTING`：拒绝新 Call，并取消尚未完成的 Call。
- `XHTTP_CLIENT_CLOSED`：Call、池传输、关闭回调和池 Timer 已经全部退出。

`xrtHttpClientDestroy()` 只释放一个公开所有者，不阻塞当前线程。最后一个公开
所有者会隐式调用 Drain，避免无人持有的 Client 继续接受连接或保留空闲池；活动
Call 仍持有内部引用并安全完成。关闭后的对象在尚有公开所有者时仍可 `Ref`、查询
状态和建立迟关闭等待，但所有新 Call 都以
`XHTTP_CLIENT_ERROR_STATE + XERR_CLOSED` 同步拒绝且不调用完成回调。

请求和响应都直接暴露底层 Header 容器：

- `xrtHttpRequestHeaderData` 从只读请求返回连续借用字段数组。
- `xrtHttpRequestTrailerData` 从只读请求返回连续借用 Trailer 数组。
- `xrtHttpRequestHeaders` 返回借用的可变 `xhttpheaders`。
- `xrtHttpResponseHeaderData` 返回连续借用响应 Header 数组。
- `xrtHttpResponseTrailerData` 返回连续借用响应 Trailer 数组。
- `xrtHttpResponseHeaders` 返回借用的只读 Header 容器。
- `xrtHttpResponseTrailers` 返回借用的只读 trailer 容器；没有收到 trailer
  时返回 `NULL`，普通响应不会为一个永远为空的容器承担分配成本。

这些入口允许通用中间件直接复用 Header 容器，不必重新实现遍历、限制或名称
比较。借用结果不得销毁，并分别在请求下次修改或响应销毁前有效。

高层错误固定使用 `xrt.http.client` 域，低层 HTTP/1 调用和无 I/O Exchange
分别使用 `xrt.http.call` 与 `xrt.http.exchange`。高层分类不会把所有执行失败
折叠成协议或 I/O 错误：

- `REQUEST`：请求正文来源、声明长度或发送计划失败。
- `STATE`：Client 已经排空、终止或关闭，不能接受新 Call。
- `RESPONSE`：响应存储、配置限额、计数或内部表示失败。
- `TRANSPORT`：HTTP 事务期间的 TCP/TLS 读写或异常关闭。
- `PROTOCOL`：对端响应语法、分帧或提前 EOF 不符合 HTTP。
- `CALLBACK`：调用方的 Informational、Headers 或 Body 回调返回 `false`。
- `DIAL`、`PROXY`、`TLS`、重定向、Cookie 和解压保留各自策略类别。

顶层 `xerrkind` 优先保留最内层有效原因。例如回调没有设置错误时是
`CALLBACK + XERR_CANCELLED`；回调设置内存错误时是
`CALLBACK + XERR_MEMORY`。完整低层原因链仍可用 `xrtErrorFind()` 检查，供
上层宿主可按稳定阶段映射错误，同时保留系统错误和具体协议错误。

## 自动重试

`http_client_retry` 是高层 Client 的独立裁剪层，复用纯协议模块
[`http_retry.md`](http_retry.md) 的状态、`Retry-After` 和退避计算。Client
默认不自动重试，必须明确设置额度或在单次调用中启用：

```c
xhttpclientconfig config;
xhttpcalloptions options;

xrtHttpClientConfigInit(&config);
config.Retry.MaxRetries = 3;
config.Retry.BaseDelay = UINT64_C(250000);
config.Retry.MaxDelay = UINT64_C(5000000);

xrtHttpCallOptionsInit(&options);
options.Retry.Mode = XHTTP_RETRY_DEFAULT;
```

`XHTTP_RETRY_DEFAULT` 继承 Client；`XHTTP_RETRY_DISABLED` 只关闭本次调用；
`XHTTP_RETRY_ENABLED` 在 Client 额度为零时使用 `XHTTP_RETRY_MAX_DEFAULT`，
否则保留 Client 额度。未知模式、未知 Flags、零 `MaxDelay` 或
`BaseDelay > MaxDelay` 在网络操作前失败。Client 配置错误使用
`XHTTP_CLIENT_ERROR_CONFIG`，调用级策略错误使用 `XHTTP_CLIENT_ERROR_RETRY`。

一次重试必须同时满足以下条件：

- 仍有 `MaxRetries` 额度；方法由 `xrtHttpMethodIdempotent()` 判定为幂等；
  存在正文时，`xhttpbody` 必须声明可重放。
- 调用方只能在确认业务允许重复副作用后，为该次调用设置
  `Options.Retry.Flags |= XHTTP_RETRY_UNSAFE`，显式授权非幂等方法。
- 状态重试默认识别 408、421、425、429、500、502、503 和 504。
- 传输重试只接受 Dial、Proxy、TLS 或事务传输阶段的临时
  `IO/AGAIN/TIMEOUT/CLOSED`；协议层只允许当前尝试在收到任何响应线路字节前
  遇到意外 EOF。已收到部分 Header 后不会重放。

中间可重试响应会先完整排空正文，再复用或关闭连接；Header 和 Body 不会进入
缓存、重定向、解压或用户回调。每次真实响应仍先经过 Cookie 层，因此中间响应的
`Set-Cookie` 可以影响下一次尝试。额度耗尽、请求不可重放或策略关闭时，最后一个
503 等状态仍作为正常 HTTP 响应交付；HTTP 状态本身不是 Client 执行错误。

有效且唯一的 `Retry-After` 优先于本地退避，并受 `MaxDelay` 封顶；非法或重复
字段回退到本地策略。默认本地延迟为封顶指数退避加 full jitter，服务端明确延迟
不再抖动。`XHTTP_RETRY_RESPECT_AFTER`、`XHTTP_RETRY_JITTER`、
`XHTTP_RETRY_STATUS` 和 `XHTTP_RETRY_TRANSPORT` 可以独立关闭。

总超时从提交起覆盖响应排空与退避等待；idle 超时只约束实际执行阶段，退避不会
消耗 idle 预算。等待期间 `Phase` 为 `XHTTP_CALL_PHASE_RETRY`，取消使用同一
Call 取消门和 Engine Timer 生命周期。Timer 真正到期并启动下一次尝试时才递增
`xhttpcallinfo.Retries` 与 `xhttpcallresult.Retries`。重试和重定向额度彼此独立，
但都属于同一个总截止时间和最终诊断快照。

## HTTPS 与会话恢复

启用 `http_client_https` 后，高层 Client 接受 `https` URL，并始终把 ALPN
固定为 `http/1.1`。默认配置使用系统信任库；嵌入式环境、测试或私有 PKI 可以
显式提供 `TlsContext` 与 `TlsVerifier`，并把 `SystemTrust` 设为 `false`。
TLS 配置把线路身份分成两个字段：

- DNS host 同时作为证书验证名称和 SNI。
- IP literal 仍作为证书验证名称，但不发送 SNI。
- 代理只决定隧道端点；目标 host 始终是 TLS 验证身份。

启用独立的 `http_client_resume` 裁剪层后，每个 Client 拥有一个有界的
TLS 1.3 ticket 缓存：

```c
xhttpclientconfig config;
xhttpresumestats stats;

xrtHttpClientConfigInit(&config);
config.Resume.MaxEntries = 64;
config.Resume.MaxEntriesPerOrigin = 4;

client = xrtHttpClientCreate(engine, &config);
xrtHttpClientResumeStats(client, &stats);
```

缓存默认最多保留 64 张票据、每条路由四张；任一上限为零都会关闭恢复缓存，
但不会关闭 HTTPS。路由键由大小写不敏感的验证 host、目标端口和可选代理对象
身份组成。host 由缓存项深复制，不借用请求、URL 或 SNI，因此 IP 地址和
无 SNI 连接仍可安全恢复。

每张 ticket 都是单次资产：拨号前从缓存摘除并转移给 TLS，拨号失败不会重新
插入，避免同一票据被并发复用。成功连接产生的新 ticket 会重新进入缓存；
HTTP 完成后才到达的 ticket 由空闲连接的 `Ticket` 事件转移，不要求为了取得
票据延迟响应完成。连接池和 ticket 缓存彼此独立：复用现有连接不消耗 ticket，
新建连接才尝试恢复。

过期项会在存入、取出和统计查询时清理。超过单路由或全局上限时按 LRU 淘汰；
缓存项分配失败只增加 `Dropped`，不会让已经成功的 HTTPS 调用失败，也不会把
内存错误遗留给调用线程。`xrtHttpClientResumeStats()` 与
`xrtHttpClientResumeClear()` 可以并发调用；统计中的 `Entries` 只计算查询时
仍有效的票据，其他计数在 Client 生命周期内累计。完整用法位于
`examples/http/client_resume/main.c`。

## 自动缓存

`http_client_cache` 是高层 Client 的独立裁剪层，同时依赖公开缓存协议原语和
`xhttpcache` 存储契约。创建 Client 前把 Store 放入配置：

```c
xhttpcache* cache = xrtHttpCacheCreate(NULL);
xhttpclientconfig config;
xhttpclient* client;

xrtHttpClientConfigInit(&config);
config.Cache.Store = cache;
client = xrtHttpClientCreate(engine, &config);
xrtHttpCacheRelease(cache);
```

Client 创建成功时已经增加 Store 引用，调用方可以立即释放自己的引用。
`xrtHttpClientCache()` 返回 Client 借用的同一句柄；没有配置 Store 时返回空指针，
且整个缓存事件层不会进入调用热路径。

`MaxBody` 是单次回源允许捕获的编码正文和最终组合表示硬上限，默认 8 MiB。
`MaxRanges` 是一次本地 byte-range-set、源站 `multipart/byteranges` 或缓存覆盖集
允许处理的片段上限，默认 16；它限制排序工作、临时数组、记录复杂度和 multipart
放大。两个值都必须大于零，Client 创建时完成配置校验。

调用级 `xhttpclientcacheoptions` 提供四种明确模式：

- `XHTTP_CLIENT_CACHE_DEFAULT`：遵守请求、响应和存储策略，允许命中和回源。
- `XHTTP_CLIENT_CACHE_DISABLED`：本次调用绕过读取、写入和失效处理。
- `XHTTP_CLIENT_CACHE_RELOAD`：强制向源站验证或重新取得表示，成功响应仍可更新缓存。
- `XHTTP_CLIENT_CACHE_ONLY`：禁止网络回源；没有可交付记录时合成 504 空响应。

请求中的 `Cache-Control: no-store`、`no-cache`、`only-if-cached`、年龄和陈旧度
约束仍由公共缓存策略解析器统一处理。调用模式不会另建一套近似语义。
`PartitionKey` 在提交时复制，用于按用户、站点或租户隔离同一个 URI；私有数据
不应只依赖 `Vary` 隔离。缓存主键至少包含方法、有效 URI、分区和记录实际声明的
`Vary` 请求字段。

响应存储遵循以下契约：

- 默认是私有缓存；`Shared` 显式切换共享缓存字段与授权规则。
- `no-store`、不可缓存状态、`Vary: *`、禁止共享保存的认证或私有字段不会进入
  Store。
- 没有显式新鲜度时，可按 `Last-Modified` 启用受限启发式寿命；默认比例为 10%，
  最长一天，可整体关闭。
  - 原始编码正文只在实际到达后按需增长，不为每个 Call 预留固定缓冲。
    `MaxBody` 默认 8 MiB，是单次回源暂存的硬上限；超限时停止缓存捕获，但网络响应
    在默认 fail-open 模式下继续正常交付。小正文从 256 B 容量起步；存储或重放
    完成后，候选记录、字段快照和捕获正文会在发布终态前释放，不随已完成 Call
    的用户引用继续驻留。
- 304 会按公共字段更新规则合并选中记录，并以旧 Record 为条件提交，再把合并后的
  完整表示通过正常事件链交付；调用方不会看到内部 304。
- unsafe 方法的成功响应会失效目标 URI，以及响应中允许失效的同源
  `Location`、`Content-Location` 表示。

304、HEAD 元数据更新和 Range 片段组合都不会用普通 `Put` 覆盖读取后的状态。
它们使用 Store 的 `Replace` 或 `RemoveRecord` 当前版本条件。304 提交发生冲突时，
当前请求仍可交付自己已经验证的不可变快照，但缓存保留并发更新者；不会为了保存
旧结果而倒退 Store。

默认 `Strict == false`。缓存解析、分配或自定义后端失败时，Client 放弃本次缓存
路径并继续处理真实网络响应；`Strict == true` 时，同一错误以
`XHTTP_CLIENT_ERROR_CACHE` 终止 Call，并保留后端或分配错误原因链。无论采用
哪种模式，协议错误都不会被伪装成正常命中。

`xhttpcallinfo.Cache` 记录最终可见响应来源：

- `MISS`：没有可用记录并发生回源。
- `HIT`：直接交付新鲜记录。
- `STALE`：按请求和响应策略允许交付陈旧记录。
- `REVALIDATED`：源站返回 304，更新并重放原记录。
- `UPDATED`：网络响应已经写入或替换记录。
- `ONLY_MISS`：禁止回源且没有可交付记录。
- `BYPASS`：模块、调用模式或请求策略明确绕过缓存。

### HEAD

HEAD 首先查找同一 URI、分区和 `Vary` 选择下的 GET 记录，未命中时才查找独立
HEAD 记录。新鲜、完整的 GET 可以直接服务 HEAD，不访问网络；响应保留表示的
状态和 Header，包括 GET 的 `Content-Length`，但不交付正文或 Trailer。
请求中的 `Range` 对 HEAD 缓存重放不起作用，不会把无正文误写成长度为零的 206
或 416。

陈旧 GET 通过 HEAD 验证时遵循公共 HEAD 更新计划：

- 304 按验证响应字段更新 GET 元数据，保留已有 GET 正文。
- 200 HEAD 的验证器和表示长度与 GET 一致时，更新元数据并保留正文。
- 200 HEAD 证明表示已经变化时，条件删除旧 GET，避免随后交付错误正文。
- 没有可更新的 GET 时，可以保存独立 HEAD 记录，但它不能用于服务 GET。

更新和删除都以先前读取的 GET Record 为条件。并发请求已经替换记录时，HEAD
不会删除或覆盖新版本。

### Range 与表示编码

完整记录支持单个或多个 `bytes` 范围、开放尾和 suffix 范围。命中时通过公共
Range 解析器裁剪、排序并合并重叠或相邻项：一个规范化区间直接构造带顶层
`Content-Range` 的 206；多个区间使用密码学安全随机边界逐段流式构造
`multipart/byteranges`，并写出精确 `Content-Length`，不会聚合第二份完整正文。
完整长度已知且所有范围不可满足时构造 416。`If-Range` 使用强 ETag 或可靠日期
验证；不匹配时忽略整个 Range 并交付完整表示。

可缓存 206 会被规范化为状态 200 的表示片段，保存完整长度和零基偏移。
后续单个缺失区间在存在强验证器时自动附加内部 `If-Range`，源站返回的兼容片段
通过公共组合计划校验验证器、长度和重叠区间后合并。一次响应携带多个片段时，
客户端先在描述符数组上完成全部计划，再一次分配最终正文、一次复制新旧覆盖并
一次提交不可变记录，不会随 Part 数量反复复制中间记录。片段无空洞覆盖完整表示
后，普通 GET 可以直接命中完整 200。验证器或表示长度冲突不会把两个表示拼在一起。
提交以参与规划的旧 Record 为条件；发生并发冲突时，客户端重新读取最新覆盖、
重新规划并重新构造 Record，而不是盲目重交旧合并结果。重试有明确上限，防止持续
写入竞争让单次 Call 无限占用 Worker，同时保证已成功提交的并发片段不会丢失。

部分记录只在每个规范化请求区间都已被现有片段覆盖时本地合成响应。单个缺失区间
可以在存在强验证器时自动附加内部 `If-Range` 并回源补齐；包含缺口的多范围请求
会保留原请求语义回源，响应验证器一致时仍可与已有覆盖原子合并。非法或重复 Range
字段、超过 `MaxRanges` 的集合，以及不能证明完整长度的片段会保守回源。
`XHTTP_CLIENT_CACHE_ONLY` 下这些路径合成 504，不会偷偷访问网络。

  源站 206 使用唯一的 `Content-Type: multipart/byteranges; boundary=...` 时，客户端
  复用公共 Multipart、Header 与缓存片段规划器逐 Part 分解。Part 可以乱序或重复；
  实现会排序、核对一致重叠并裁去重复字节，同时要求每个 `Content-Range`、可选
`Content-Length`、已知完整长度和已出现的 `Content-Type` 相互一致，并拒绝 Part
传输编码。保存时删除外层 framing 的 `Content-Type`、`Content-Length` 与
  `Content-Range`，恢复 Part 描述的表示类型；完整覆盖时写回表示的精确长度。
  Part 描述数组按实际数量从小容量增长，`MaxRanges` 只是硬逻辑上限，不会被直接
  换算成每次响应的预分配大小。
畸形、矛盾或超过限制的 multipart 在宽松模式下仍原样交付网络响应但不写缓存，
Strict 模式则以 `XHTTP_CLIENT_ERROR_CACHE` 终止并保留底层 Value、Range 或 Memory
原因。

自动解压不会为带 `Range` 的请求隐式添加 `Accept-Encoding`，避免把表示字节
区间误当作解码后字节区间。缓存记录带有非空 `Content-Encoding` 且调用启用了
自动解压时，也不会在本地切片。需要明确控制编码表示范围的调用方应自行设置
`Accept-Encoding`，并使用 `XHTTP_DECOMPRESS_RAW` 保留线路字节。

缓存命中在连接池和 DNS 前完成，但仍通过重定向、解压、诊断和用户事件链交付，
因此回调、Future、同步和协程入口观察到相同响应契约。缓存记录不会重放
`Set-Cookie` 副作用；真实网络响应的 Cookie 处理仍先于缓存存储和重定向决策。

完整配置示例位于 `examples/http/client_cache/main.c`。缓存协议、存储后端和
Range 组合的底层用法分别见 `http_cache.md`、`http_cache_store.md` 与
`http_cache_range.md`。

### 历史资产

旧版没有客户端自动缓存实现，因此 `http_client_cache` 没有可直接迁移的旧文件，
不是跳过了历史审计。该层复用已经从旧 `xweb` 静态响应中提炼并验证的 ETag、
条件请求、Range、206/304/416 和文件片段边界，再组合新版公开缓存策略、验证、
片段覆盖与存储契约。旧版明确不支持的多范围请求由公共 Range 与
`multipart/byteranges` 层补齐，相关旧测试边界已增强迁入各依赖模块，本层只保留
客户端命中、回源、部分覆盖、编码表示和故障注入的端到端证据。

## Future、同步与协程

`http_client_future` 是独立裁剪层，只组合 `http_client` 与通用 Future 适配桥。
回调核心仍然是零额外结果对象的最低成本入口；需要任务组合、同步便利入口或
协程等待时，再启用 `XHTTP_FEATURE_HTTP_CLIENT_FUTURE`。

```c
xfuture* future = xrtHttpClientDoAsync(client, request, &options);

if ( xrtFutureWait(future) == XWAIT_OK ) {
	xhttpresult* result = (xhttpresult*)xrtFutureValue(future);
	const xhttpresponse* response = xrtHttpResultResponse(result);

	printf("%u\n", (unsigned)xrtHttpResponseStatus(response));
}
xrtFutureDestroy(future);
```

同一裁剪层还提供可取消的 Client 关闭等待：

```c
xfuture* closed = xrtHttpClientWaitAsync(client);

xrtHttpClientDrain(client);
xrtHttpClientDestroy(client);

if ( xrtFutureWait(closed) == XWAIT_OK ) {
	/* Client 的 Call 回调、池传输和池 Timer 已经全部退出。 */
}
xrtFutureDestroy(closed);
```

每次 `xrtHttpClientWaitAsync()` 建立独立等待。取消其 Future 只摘除当前等待者，
不会 Drain、Abort 或影响其他等待者。`CLOSED` 后建立的等待立即成功；关闭 Future
在最后一个 Call 完成回调返回、连接池关闭回调返回且池清扫 Timer 的取消回调退出后
才完成，因此调用方不需要轮询 Engine 对象计数。Future 持有内部 Client 引用，允许
先建立等待、再释放最后一个公开所有者，最后等待确定性资源退出。

Future 成功值是其拥有的 `xhttpresult`，不是单独的 Response。这样普通响应、
重定向元数据、`101` 或 CONNECT Upgrade、明文/TLS 传输和已缓冲的协议外字节
使用同一结果契约，不会因为选择便利 API 丢失底层能力。

- `xrtHttpResultResponse`、`Tcp`、`Tls` 返回借用值。
- `xrtHttpResultTakeResponse`、`TakeTcp`、`TakeTls` 原子取走对应所有权；
  同一结果的 Take 与借用访问不能并发。
- 最后一个结果引用会销毁未取走的 Response，并中止、释放未取走的 Upgrade
  传输。需要让结果超过 Future 生命周期时，先调用 `xrtHttpResultRef`。
- `xrtHttpResultBuffered` 返回 Upgrade 时留在传输接收缓冲中的字节数，
  `xrtHttpResultRedirects` 返回已经跟随的重定向次数。
- `xrtHttpResultInfo` 复制完成时冻结的 `xhttpcallinfo`，销毁底层 Call 后仍然
  有效。

取走 Upgrade 的 `Tcp` 或 `Tls` 后，下一协议处理器必须在该 Stream 所属
Worker 上安装事件，并先消费已经留在接收缓冲中的 `Buffered` 字节。销毁结果
只会中止尚未取走的传输；取走后，关闭、半关闭和最终释放均由接管方负责。

取消 Future 会协作取消底层 Call；调用选项中的外部 `xcancel` 或
`xrtHttpCallCancel` 使 Call 取消时，Future 进入 `XFUTURE_CANCELLED`。等待函数
返回 `XWAIT_OK` 只表示 Future 已经进入终态，操作结果仍应通过
`xrtFutureState`、`xrtFutureResult` 或 `xrtFutureValue` 判定。

宿主线程可用 `xrtHttpClientDoSync` 阻塞等待同一个 Future 契约并取得拥有型
结果。网络 Worker 上调用同步入口会在提交请求前以
`XHTTP_CLIENT_ERROR_INTERNAL` 失败，避免 Worker 等待自身产生死锁。

启用 `XRT_FEATURE_FUTURE_COROUTINE` 后，协程直接使用通用
`xrtFutureAwait`、`AwaitFor` 或 `AwaitUntil`：

```c
xfuture* future = xrtHttpClientDoAsync(client, request, &options);

if ( xrtFutureAwait(future) == XWAIT_OK ) {
	xhttpresult* result = (xhttpresult*)xrtFutureValue(future);
	/* 使用 result。 */
}
xrtFutureDestroy(future);
```

协程等待被取消或超时只会摘除当前 waiter，不会自动取消可能被其他消费者共享
的 Future。协程确定不再需要这次 HTTP 调用时，应明确调用
`xrtFutureCancel(future)`；也可以把协程作用域的 `xcancel` 放入调用选项，使
取消从提交时就进入统一 Call 契约。

## 常用请求便利层

`http_client_easy` 不创建隐藏 Engine、Client 或第二套请求实现。它只在调用期间
创建一个临时 `xhttprequest`，再交给 `xrtHttpClientDo()` 冻结，因此超时、取消、
重定向、Cookie、缓存、代理、TLS、解压、诊断和错误链与底层入口完全一致。提交
返回后临时请求立即销毁，固定正文只由 `xrtHttpRequestSetBytes()` 复制一次，
异步快照共享同一个 `xhttpbody` 引用。

callback 层提供三组常用入口：

- `xrtHttpClientGet()` 提交无正文 GET，不人为创建空正文。
- `xrtHttpClientPost()` 复制固定字节正文并提交 POST。
- `xrtHttpClientSendBytes()` 允许 PUT、PATCH 或其他合法方法携带固定字节正文。

```c
call = xrtHttpClientPost(
	client,
	XRT_STR_LITERAL("https://api.example.test/items"),
	XRT_BYTES_LITERAL("{\"name\":\"xrt\"}"),
	XRT_STR_LITERAL("application/json; charset=utf-8"),
	NULL,
	on_done,
	context
);
```

启用独立的 `http_client_easy_future` 后，同一组操作增加 `Async` 与 `Sync` 后缀：

```c
xhttpresult* result = xrtHttpClientPostSync(
	client,
	XRT_STR_LITERAL("https://api.example.test/items"),
	XRT_BYTES_LITERAL("{\"name\":\"xrt\"}"),
	XRT_STR_LITERAL("application/json; charset=utf-8"),
	NULL
);
```

`GetAsync`、`PostAsync` 和 `SendBytesAsync` 返回普通 `xfuture`；`GetSync`、
`PostSync` 和 `SendBytesSync` 返回普通拥有型 `xhttpresult`。同步入口仍禁止在网络
Worker 上阻塞。`ContentType` 为空时不生成 `Content-Type`；POST/SendBytes 的
空字节视图表示显式零长度正文，与 GET 的“没有正文”保持区别。

便利入口故意不接受另一套 Header、流式正文或表单配置结构。需要设置认证和自定义
Header、发送文件或生产者流、组合 FormData、控制 Upgrade 时，使用
`xrtHttpRequestCreate()`、`xrtHttpRequestSetBody()` 和 `xrtHttpClientDo*()`；这些
底层入口始终公开且与便利层并列。完整示例位于
`examples/http/client_easy/main.c`。只需要 callback 便利函数时选择
`XHTTP_MODULE_HTTP_CLIENT_EASY`，Future/同步版本再选择
`XHTTP_MODULE_HTTP_CLIENT_EASY_FUTURE`。

## 代理

`http_client_proxy` 是独立裁剪层，它把高层 Call 接到现有
`xrtNetProxyDial`，不复制 CONNECT 或 SOCKS5 状态机。Client 创建时保留
`xhttpclientconfig.Proxy` 的默认代理引用；创建成功后，调用方可以立即释放
自己的引用。`xrtHttpClientProxy` 返回借用的默认代理，未配置时返回空指针且
不设置错误。

每次调用通过 `xhttpproxyoptions` 明确选择：

- `XHTTP_PROXY_DEFAULT`：继承 Client 默认代理，`Proxy` 必须为空。
- `XHTTP_PROXY_DIRECT`：本次调用绕过 Client 默认代理，`Proxy` 必须为空。
- `XHTTP_PROXY_EXPLICIT`：本次调用使用并保留非空 `Proxy`。

选择在提交成功前冻结，代理对象、请求和选项随后都可以由调用方释放或修改。
代理选择覆盖完整重定向链，不会因为换 origin 自动切换为直连。当前
`xnetproxy` 表达隧道代理，因此 CONNECT 与 SOCKS5 都把目标 host 交给代理
端解析；应用 HTTP 请求始终使用 origin-form，代理认证只进入握手，不写入
目标请求 Header。

HTTPS 在隧道建立后才对目标执行 TLS。SNI、证书验证名称和 ALPN 都使用目标
host，不使用代理 host。总截止时间覆盖代理 DNS、TCP、代理握手、隧道内 TLS
和 HTTP 事务；这些阶段长期没有完成也受空闲截止时间约束。取消会协作终止
当前阶段。`407` 等失败在顶层使用
`xrt.http.client / XHTTP_CLIENT_ERROR_PROXY`，底层 `xrt.net` 原因链保持完整。

连接池键在同一 Client 内至少包含 scheme、host、port 和代理对象身份。直连、
不同代理以及 HTTP/HTTPS 永不共用连接；即使两个代理对象配置相同，也按不同
身份保守隔离。TLS Context 与验证器在 Client 创建后固定，因此不会在该 Client
内部出现混合 TLS 策略。

统一代理示例位于 `examples/http/client_proxy/main.c`：

```text
client_proxy <http-connect|socks5> <proxy-host> <proxy-port> <http-url> [--direct]
```

`http_client_proxy_http_connect` 与 `http_client_proxy_socks5` 是两个独立聚合
模块；只选择其中一个时不会携带另一个协议后端。真实 SOCKS5 高层组合测试覆盖
RFC 1929 用户名密码认证、目标域名由代理解析、origin-form 请求、正确的
`Host`，以及代理认证不泄漏到应用 Header。CONNECT 组合测试覆盖连接池、拒绝、
超时、取消、隧道内 TLS、目标 SNI/ALPN 和 Select/IOCP。确定性单次分配失败
测试在代理 Dial 创建点注入 OOM，要求 Call 只完成一次、保留
`XHTTP_CLIENT_ERROR_PROXY -> XERR_MEMORY` 原因链，且 Engine 活动对象归零。

## 连接池

启用 `http_client_pool` 后，连接池按 origin 管理活动、等待和空闲传输。
默认不限制活动连接，最多保留 128 条全局空闲连接、每 origin 8 条，并在
90 秒后清扫。任一空闲上限为零都会关闭复用；等待上限为零表示不限制，
不是拒绝全部等待者。

`xrtHttpClientPoolConfigInit` 支持未对齐的完整结构存储，并拒绝发生地址环绕的
输出范围。

origin 键由 scheme、ASCII 不区分大小写的 host、port 和代理对象身份组成；
TLS Context 与验证器由 Client 固定。连接池按 Engine Worker 数建立最多 32 个固定
Origin 分片，每个分片独立维护 Origin 索引、等待 FIFO、空闲 LRU 和一条清扫 Timer，
不为每条连接建立独立 Timer。获取、复用和同 Origin 交接只访问一个分片；全局连接、
等待和空闲硬上限使用原子配额协调，不需要 Client 级热锁。

等待队列在同一分片内按“最早可运行”保持 FIFO 公平。连接归还时，同 Origin
等待者可直接接管传输；如果其他分片存在等待者且全局连接额度已经用尽，则当前
连接让出槽位，并从相邻分片开始轮转分发。全局槽位释放、空闲超时、对端关闭和
显式清理都会触发跨分片唤醒，不能让某个分片仅因没有本地完成事件而长期休眠。

池不会使用 HTTP/1 pipelining，一条连接同一时刻只执行一个事务。只有消息
边界完整、没有 `Connection: close`、没有升级且传输保持健康的连接才会回池。
总截止时间在排队期间继续计时；Call 开始执行后，连接池等待也受空闲截止时间
约束。`xrtHttpClientCloseIdle` 只摘除空闲连接，不打断活动 Call；
`xrtHttpClientStats` 返回可并发读取的当前数量和单调生命周期计数，各字段不
承诺来自同一个全局时刻。`ActiveConnections` 包含正在拨号、使用或已保留配额
的连接，不包含已经从配额中摘除、仅等待异步 Close 回调的连接。
Client Drain/Abort 会调用同一空闲关闭路径；生命周期终态还会等待这些异步 Close
和全部活动分片 Timer 的取消回调，不把“已经从 LRU 摘除”误报为“已经完全关闭”。

Origin 创建失败会以 `XHTTP_CLIENT_ERROR_POOL` 保留 `XERR_MEMORY` 原因且不
提交任何配额。响应已经成功后，如果空闲项或清扫 Timer 无法建立，Client 只
关闭该传输，不把池优化失败改写为请求失败。额外输入、FIN、TLS `close_notify`
和复用前健康检查都会淘汰陈旧连接。

完整配置与统计示例位于 `examples/http/client_pool/main.c`。真实 Select/IOCP
测试覆盖直接复用、等待取消与拒绝、跨 origin 公平、被阻塞队首、空闲过期、
额外字节、半关闭和关闭回调竞态；独立 OOM 测试覆盖 Origin 深复制的事务回滚。

## 认证

认证能力按通用语法、Basic、Bearer 和响应 challenge 分别裁剪。常见请求不需要手工拼接 Header：

```c
xrtHttpRequestSetBasicAuth(
	request,
	XRT_STR_LITERAL("Aladdin"),
	XRT_STR_LITERAL("open sesame")
);

xrtHttpRequestSetBearerAuth(
	request,
	XRT_STR_LITERAL("mF_9.B5f-4.1JqM")
);
```

自定义方案使用 `xrtHttpRequestSetAuth`；代理凭据使用名字中带 `Proxy` 的对称入口。所有设置函数都生成唯一字段并清除构建阶段的临时凭据副本。`xrtHttpRequestClearAuth` 与 `xrtHttpRequestClearProxyAuth` 返回实际删除数量。

401/407 响应可能包含多条认证字段，每条字段又可能包含多个 challenge。初始化一个 `xhttpauthcursor` 后，使用 `xrtHttpResponseChallengeNext` 或 `xrtHttpResponseProxyChallengeNext` 迭代通用 `xhttpauth`，直至返回 `XHTTP_NEXT_END`。游标和结果描述符允许未对齐存储，语法错误不推进游标，并进入 `xrt.http.client.response` 错误域和 `XHTTP_RESPONSE_ERROR_AUTH`，不会静默跳过。

Basic、Bearer 与 Digest 分别提供独立裁剪的结构化响应入口：

```c
xhttpauthcursor cursor;
xhttpbasicchallenge basic;
char realm[128];
size_t size;

xrtHttpAuthCursorInit(&cursor);
while ( xrtHttpResponseBasicChallengeNext(
	response,
	&cursor,
	realm,
	sizeof(realm),
	&size,
	&basic
) == XHTTP_NEXT_ITEM ) {
	/* basic.Realm 借用 realm，basic.Utf8 表示 charset=UTF-8。 */
}
```

`xrtHttpResponseBasicChallengeNext`、`xrtHttpResponseBearerChallengeNext`、`xrtHttpResponseDigestChallengeNext` 以及带 `Proxy` 的对称入口会跨字段跳过其他认证方案，只解码匹配 scheme。传入空输出和零容量是长度查询：函数完成全部协议验证并发布所需字节数和不依赖输出区的事实，但故意不推进游标；调用方可据此精确分配并重试同一 challenge。提供实际缓冲且成功后才提交游标。短缓冲、畸形匹配项、无效参数与 OOM 都不消费当前 challenge；短缓冲在 `pSize` 中发布所需长度。

Digest 的常用路径可直接调用 `xrtHttpResponseDigestChallengeChoose`。它从头遍历全部 `WWW-Authenticate` 字段和字段内 challenge，跳过其他 scheme 以及本地不支持的 Digest 算法，按服务器线路偏好返回第一个满足 `xhttpdigestpolicy` 的 challenge 与 `xhttpdigestchoice`。代理使用 `xrtHttpResponseProxyDigestChallengeChoose`。空输出仍执行完整候选验证并返回精确解码长度；畸形 Digest 候选不会被静默跳过。需要自行决定候选优先级时继续使用游标入口。

选定 challenge 后，使用 `xrtHttpDigestSecret` 得到 `H(A1)`，再由 `xrtHttpDigestClientAuth` 一次构造 `xhttpdigestauth`，最后调用 `xrtHttpRequestSetDigestAuth`。这三层分别负责密码派生、无状态协议计算和请求对象字段设置；都不保存密码、nonce 计数或重放状态。代理凭据使用 `xrtHttpRequestSetProxyDigestAuth`。

并发客户端通常使用 `xhttpdigestsession` 保存 nonce 状态。`http_client_prepare_auth_digest_session` 把会话接到 HTTP/1 请求冻结层，并在准备器确定最终线路 request-target 后才生成凭据：

```c
xhttpdigestexchange* exchange = NULL;
xhttp1requestplan* plan = xrtHttp1RequestPrepareDigest(
	request,
	&options,
	session,
	entity_hash,
	&exchange
);
```

该入口覆盖 origin-form、absolute-form、authority-form、asterisk-form 和显式 custom target，Digest 的 `uri` 与请求行逐字节一致。它不修改请求构建器；已有 `Authorization` 只在冻结计划中被唯一的新字段替换，代理版本 `xrtHttp1RequestPrepareProxyDigest` 对称处理 `Proxy-Authorization`，另一类凭据不受影响。成功后分别销毁 Plan 和释放 Exchange。失败不会发布半成品；若错误发生在 Exchange 创建之前则不消耗 `nc`，若凭据已经保留而后续 Header 或 Plan 分配失败则允许计数跳号，这符合 nonce-count 只要求严格递增的协议契约。

成功响应中的唯一 `Authentication-Info` 由 `xrtHttpResponseDigestInfo` 读取；代理回执使用对称入口。客户端为每个已发送请求保留实际 `xhttpdigestproof`，再调用 `xrtHttpDigestInfoVerify` 校验 rspauth、qop、cnonce 和 nc。并发响应可以乱序完成，不能使用一个全局“最后请求”上下文。只有证明有效后才采用 `nextnonce`。重复回执字段、部分证明组或错误摘要均返回错误或 `INVALID`，不会静默选择其中一条。

使用会话时可以把上述步骤收敛成一次响应操作：

```c
xhttpdigestsessioncheck check;
xhttpnext next = xrtHttpResponseDigestSessionAccept(
	response,
	session,
	exchange,
	response_entity_hash,
	next_cnonce,
	&check
);
```

`END` 表示响应没有 `Authentication-Info`，由应用决定是否允许缺少双向证明；`ERROR` 表示重复或畸形字段、参数、OOM 或会话失败；`ITEM` 表示读到了唯一合法结构，此时 `check` 再区分 `INVALID`、`VALID`、`UPDATED` 和 `SUPERSEDED`。只有 `UPDATED` 会采用当前 Exchange 的 `nextnonce`；乱序到达的旧 Exchange 即使证明有效也只返回 `SUPERSEDED`。代理使用 `xrtHttpResponseProxyDigestSessionAccept`。适配器按字段实际解码长度分配，不使用固定响应缓冲，解码缓冲和会话状态两个分配点均有失败原子性测试。

结构化入口不隐藏通用层。需要读取未知 scheme、认证扩展参数或保留原始认证数据时，继续使用通用迭代器和 `xrtHttpAuthParamNext`。challenge 和原始回执读取适配层不分配内存，也不把重复 `WWW-Authenticate` 或 `Proxy-Authenticate` 合并成一个字段；拥有型请求计划与会话回执适配器按实际长度分配并具有逐点 OOM 原子性测试。

跨 origin 重定向默认移除 `Authorization` 和 `Proxy-Authorization`；只有调用方显式选择转发凭据时保留。应用仍负责只在满足机密性要求的传输上发送凭据，并避免记录完整认证字段。

## 重定向

启用 `http_client_redirect` 后，Client 默认最多跟随十跳，并按常见客户端语义
把 301、302、303 后的 POST 改为 GET；307 与 308 保留方法和正文。需要保留
正文的方法重定向必须具有可重放正文，否则以
`XHTTP_CLIENT_ERROR_REDIRECT_REPLAY` 失败。

HTTP 方法按协议要求区分大小写。上述 POST 改写仅匹配标准方法 `POST`；例如
自定义方法 `post` 不会被当作 POST，其方法和可重放正文会在 301、302 中保留。

相对 `Location` 以当前有效 URL 解析。跨 origin 默认移除 `Authorization`、
`Proxy-Authorization` 与 `Cookie`；只有显式
`XHTTP_REDIRECT_FORWARD_CREDENTIALS` 才允许保留。HTTPS 到 HTTP 的降级默认
拒绝，可由 `XHTTP_REDIRECT_ALLOW_DOWNGRADE` 明确开放。每次调用可选择继承、
跟随、返回原响应或把重定向视为错误。整个链共享同一个总截止时间。

HTTP 重定向保留 RFC 9110 的 fragment 语义：`Location` 没有 `#` 时继承当前
有效 URL 的 fragment；`Location` 含显式空 `#` 时阻止继承。fragment 只保留在
最终有效 URL 中，不进入 HTTP request-target。POST 改写为 GET 时会同时删除
正文 framing、表示类型、Content-Disposition 以及旧版和现代 Digest 字段。
Call 发布成功、失败、取消或超时终态前会释放尚未提交的下一跳请求快照。

## 自动解压

`http_client_decompress` 是依赖公开 `inflate` 模块的独立裁剪层。模块存在时，
Client 默认启用自动内容解码；`XHTTP_DECOMPRESS_DEFAULT` 继承 Client，
`XHTTP_DECOMPRESS_AUTO` 强制启用，`XHTTP_DECOMPRESS_RAW` 保留线路表示。

自动模式只在请求没有显式 `Accept-Encoding` 且没有 `Range` 时添加
`Accept-Encoding: gzip, deflate`。调用方显式字段永远不会被覆盖；若仍选择
自动模式，受支持的返回编码照常解码，需要完整原始表示或自行处理编码表示范围时
应选择 `RAW`。

接收契约如下：

- 支持 `gzip`、兼容别名 `x-gzip` 和兼容 zlib/raw 的 `deflate`；
  `identity` 作为无变换层容错接收。
- 重复 `Content-Encoding` 字段按到达顺序合并，列表中的空元素按接收方规则
  忽略，实际解码按编码应用顺序的逆序执行。
- 只要出现未知 coding，就不做部分解码，而是完整保留原始正文和
  `Content-Encoding`、`Content-Length`。
- 已支持 coding 的列表语法错误、损坏或截断数据、输出超限都会让 Call 以
  `XHTTP_CLIENT_ERROR_DECOMPRESSION` 失败，并保留 `XERR_PROTOCOL`、
  `XERR_RANGE` 或 `XERR_MEMORY` 原因。
- HEAD、1xx、204、205、304、成功 CONNECT 和协议升级响应不建立解码器。
- 自动重定向先过滤中间响应，再为最终可见响应建立解码器；隐藏的 3xx 正文
  只排空，不消耗解码内存。

成功解码后，响应移除已经失效的 `Content-Encoding` 与 `Content-Length`，
设置 `XHTTP_RESPONSE_DECOMPRESSED`，并通过
`xrtHttpResponseOriginalEncoding()` 保留合并后的原始编码文本。
`xrtHttpResponseBodyBytes()` 和 `xhttpcallinfo.ResponseBodyBytes` 记录最终
明文字节；`xrtHttpResponseWireBodyBytes()` 记录最终响应的编码正文载荷；
`xhttpcallinfo.ResponseWireBytes` 累计完整重定向链的 HTTP 线路输入。
缓冲、流式回调、Future、同步和协程入口共享这组语义。

默认最多输出 64 MiB 明文并接受四个 coding token，编译期硬上限是十六层。
每个实际 Inflate 解码器按需分配一个约 32 KiB 字典，只在最终响应 Header
确认全部编码受支持后创建；Client、连接和空响应不预留解压缓冲。`MaxBody`
同时约束每个中间解码层和最终输出，防止叠加编码绕过解压炸弹限制。
客户端解码器使用同步推送链，上一层输出直接进入下一层或用户 Body 回调，不建立
中间队列，也不为每个连接预留固定正文缓冲。用户可在 Header 或 Body 回调内通过
`xrtHttpCallPause()` 施加背压；恢复后继续沿同一解码状态交付。

## 自动 Cookie

`http_client_cookies` 是独立裁剪层。把可选 `xcookiejar*` 放入
`xhttpclientconfig.Cookies` 后，Client 在创建时增加引用；创建成功后调用方
可以立即释放自己的 Jar 引用。`xrtHttpClientCookieJar` 返回 Client 借用的
同一 Jar，未配置时返回空指针且不设置错误。

默认调用是同站、非顶层、未分区的 HTTP API 请求。逐次调用可通过
`xhttpcookieoptions` 调整：

- `XHTTP_COOKIE_DISABLED`：本次调用既不自动发送，也不接收 Cookie。
- `XHTTP_COOKIE_SAME_SITE`：声明当前请求与 Cookie 上下文同站。
- `XHTTP_COOKIE_TOP_LEVEL`：声明顶层导航。
- `PartitionKey`：由 Call 复制的分区键，空视图表示未分区请求。

`SAME_SITE` 与 `TOP_LEVEL` 描述整个调用及其重定向链的站点上下文，同时用于
请求 Cookie 选择和响应 `Set-Cookie` 接收，不是由 Client 根据 origin 猜测。
站点与 origin 不是同一概念；调用方在跨站上下文中必须清除 `SAME_SITE`，需要
浏览器级动态上下文时可在更高层按每次导航创建 Call。跨站子资源响应只能创建
`SameSite=None; Secure` Cookie，顶层导航仍可创建其他 SameSite 模式。

方法安全性由 `xrtHttpMethodSafe` 统一判定。请求已经显式设置 `Cookie` 时，
该字段完全由调用方控制，Jar 不覆盖它。自动生成的字段则在每一跳先移除，
再按新 URL、方法、SameSite 与分区键重新选择，不能把上一 origin 的结果直接
转发。跨 origin 重定向移除显式凭据后，Jar 可以为新 origin 正常重新选择。

每一跳非信息响应的全部独立 `Set-Cookie` 会在重定向判断和用户 Header 回调
之前写入 Jar，因此中间 3xx 响应设置的 Cookie 可参与下一跳，最终响应设置的
Cookie 在用户回调中已经可见。1xx 信息响应不修改 Jar，也不触发最终 Header
回调。Call 进入任意终态后会在完成回调前释放自己复制的分区键，不让长期保留的
已完成 Call 占用策略内存。

任一 Cookie 选择或存储运行时错误都以 `XHTTP_CLIENT_ERROR_COOKIE` 结束；
原因链中的内存不足即使经过 Exchange 回调包装，仍保持 `XERR_MEMORY` 分类。
显式 Cookie、手工 CookieJar API 和自动策略可以并存，没有强制对象构建路径。

示例位于 `examples/http/client_cookies/main.c`。正常、禁用、显式覆盖、重定向
逐跳重选、SameSite 安全方法矩阵、CHIPS 分区隔离、1xx、Select/IOCP 和三条
OOM 边界均有独立测试。
