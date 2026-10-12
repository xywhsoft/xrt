# HTTP 服务端

HTTP 服务端按协议核心、网络运行时和可选便利层拆分。`http_server_exchange`
提供无 I/O 的单连接串行 HTTP/1 请求状态机，`http_server_request` 提供拥有型
入站请求快照，`http_server_reply` 提供可选的响应构建器。Server、Future、
TLS、文件与 WebSocket Upgrade 模块都在相同契约上继续组合。

## Server Exchange

启用 `XHTTP_FEATURE_HTTP_SERVER_EXCHANGE` 需要 `HTTP_SERVER_REQUEST`、
`HTTP_HOST`、`HTTP_TARGET`、`HTTP_EXPECT` 与 `HTTP_TE`。Exchange 不拥有 Socket、线程、Timer 或连接接收缓冲，只同步
消费调用方交付的字节。`Feed` 的 `Accepted` 是可以从连接缓冲移除的精确前缀；
完整请求后的流水线后缀、暂停后的未交付正文和拒绝后的全部正文都不会被提前
接受。

`Accepted` 支持合法的未对齐存储。输入和输出都必须是完整、不回绕的范围，
输出不得覆盖输入，二者也不得借用 Exchange 或当前请求快照的内部存储。
参数错误不会推进解析状态；修正参数后可以继续使用同一 Exchange。

请求头跨 `Feed` 边界时才建立按实际长度增长的临时缓冲；单段完整请求头直接
解析，不预留固定 8 KiB。字段描述符按实际字段数建立，请求快照完成后立即
释放。正文默认上限为 4 MiB，`Headers` 回调可以按路由调用
`xrtHttp1ServerExchangeSetBodyLimit` 提高、降低或取消限额，然后选择：

- `XHTTP_SERVER_BODY_BUFFER`：按实际正文增长拥有型连续存储。
- `XHTTP_SERVER_BODY_STREAM`：同步交付借用片段，不保存正文副本。
- `XHTTP_SERVER_BODY_DISCARD`：完整排空正文与 Trailer，但不分配正文存储，也不调用正文回调。
- `XHTTP_SERVER_BODY_REJECT`：只接受请求头，由网络层决定发送错误响应或关闭。

`xrtHttp1ServerConfigInit` 支持未对齐的完整结构存储。Exchange 创建在分配对象前
验证并复制配置与可选事件表；创建成功后不再借用这两个描述符，回绕地址会以
`XERR_ARGUMENT` 同步拒绝。

`AllowRawTransferCodings` 默认为 `false`，因此服务器不会把仍带 `gzip` 等
Transfer Coding 的数据静默当作普通正文。代理或已经安装自定义传输解码器的
应用可以显式开启该项；Exchange 只移除最终 chunked 分帧，其余编码按原样交付，
编码顺序可通过 `xrtHttp1TransferCodingNext` 读取。

流式消费者可在 `Headers` 或 `Body` 回调调用 `Pause`。当前已交付片段属于
已接受前缀，后续字节由连接缓冲保留；`Resume` 后从原后缀继续 `Feed`。
固定长度正文的最后一个片段也允许在 `Body` 回调内暂停，此时恢复后用空输入
再次 `Feed`，再发布 `Complete`，不会让异步消费者与请求完成事件竞态。
Trailer 在完整严格解析后复制进请求快照。`Complete` 只执行一次，此后应用
完成响应并调用 `Next`，仅 keep-alive 请求允许复用 Exchange。

```c
xhttp1serverevents events = {
	NULL,
	NULL,
	onComplete,
	context
};
xhttp1serverexchange* exchange =
	xrtHttp1ServerExchangeCreate(NULL, &events);
size_t accepted = 0;
xhttp1serverfeedstatus status =
	xrtHttp1ServerExchangeFeed(
		exchange,
		input,
		false,
		&accepted
	);
```

协议核心强制执行方法与四种 request-target 的合法组合、HTTP/1.1 唯一有效
`Host`、HTTP/1.0 至多一个 `Host`、
严格 `Expect` 与 `TE` 列表语法、严格 CRLF、Header/字段/正文/chunk/Trailer
限额、禁止 Trailer 字段，以及 `Transfer-Encoding` 与 `Content-Length`
防走私规则，并默认拒绝未实现的 Transfer Coding。同一个 Exchange 不允许从事件回调重入 `Feed`；回调只可使用明确
允许的 `Pause`、`Resume`、正文限额和只读查询。所有失败进入稳定终态，错误域
为 `xrt.http.server.exchange`，回调错误作为原因链保留。

重复字段和同一字段中的多个 `100-continue` 按 HTTP 列表规则合并处理。只有
HTTP/1.1 非零定长或 chunked 请求发布一个 Continue 握手事实；零正文和 HTTP/1.0
请求中的 `100-continue` 会被解析并忽略。语法正确但服务器不支持的扩展 expectation
进入 417 路径，畸形值保留底层解析错误原因链。

重复 `TE` 字段同样按列表规则组合。畸形值进入稳定协议错误；只有 HTTP/1.1 请求同时包含
裸 `trailers` 成员和 `Connection: TE` 时，Exchange 才设置
`XHTTP_SERVER_REQUEST_ACCEPTS_TRAILERS`。`xrtHttpServerRequestAcceptsTrailers` 提供直接
查询，缺少逐跳声明或 HTTP/1.0 请求不会让高层响应代码误发重要 Trailer；原始 Header
仍完整保留，低层代理可以自行实施不同策略。

## 请求快照

启用 `XHTTP_FEATURE_HTTP_SERVER_REQUEST` 需要 `HTTP1_BODY` 与 `HTTP_TARGET`。请求行、Header
描述符和实际字段文本保存在一个紧凑分配块中；常见单包 Header 不需要先建立
8 KiB 接收对象，也不为不存在的正文或 Trailer 预留空间。请求使用引用计数，
服务器异步处理期间可以保留同一个对象。

`Method` 与 `Target` 保留线路原文，Header 和 Trailer 通过大小写不敏感名称
查找。`BodyMode` 与 `ContentLength` 来自唯一 HTTP/1 分帧结论，不由应用重新
解释 Transfer-Encoding 或 Content-Length。

`xrtHttpServerRequestHeaderData` 与 `xrtHttpServerRequestTrailerData` 公开连续只读
字段数组，数量分别由对应的 `Count` API 返回。数组和名称值视图都借用请求快照，
适合直接复用 `xrtHttpFieldFind`、`xrtHttpFieldGetUnique`、列表解析器或自定义协议层；
调用方不得修改或释放。

Reply 对 Header 和 Trailer 提供完全相同的 `Count + Data + At + container` 读取
口径。`xrtHttpReplyHeaderData` 与 `xrtHttpReplyTrailerData` 不触发惰性容器创建，
空 Reply 或空字段集返回空指针。

`xrtHttpServerRequestParseTarget` 公开无分配的结构化 target 视图；
`xrtHttpServerRequestAuthority` 直接返回路由应使用的有效 authority。
absolute-form 与 CONNECT 从 target 取 authority，origin-form 与 `OPTIONS *`
从 Host 字段取值。底层代理可以继续读取原始 Target，高层路由不需要重复实现
URI、IPv6、端口或 Host 覆盖规则。

缓冲模式只按实际收到的正文增长连续内存；存在正文时，流式和丢弃模式都设置
`XHTTP_SERVER_REQUEST_STREAMED`，只累计 `BodyBytes`，`Body` 始终为空。丢弃模式还设置
`XHTTP_SERVER_REQUEST_DISCARDED`，用于与真正交付给 `Body` 回调的流式请求区分。
Header、正文片段与 Trailer 的推进由所属 Worker 串行完成；跨线程读取仍应在
请求进入 `XHTTP_SERVER_REQUEST_COMPLETE` 后进行，或只读取已经固定的请求头
元数据。

### 请求数据便利层

请求快照只负责拥有协议事实；Query、Cookie、Content-Type 和表单解析分别由
独立裁剪模块组合，不把所有协议工具强制拖入 HTTP Server 核心：

- `XHTTP_FEATURE_HTTP_SERVER_QUERY` 提供 `xrtHttpServerRequestQueryParams`，先通过
  公共 request-target 解析器取得 Query，再返回拥有型 `xqueryparams`。没有 Query
  时返回有效的空容器，调用方用 `xrtQueryParamsDestroy` 释放。
  - `XHTTP_FEATURE_HTTP_SERVER_COOKIE` 提供 `xrtHttpServerRequestCookies` 与
    `xrtHttpServerRequestCookie`。前者无分配地聚合全部 `Cookie` 字段，可先用空数组
    查询总数，再一次取得按线路顺序排列的借用项，适合一次请求内进行多键查找；
    后者严格验证全部字段后直接返回首个同名项，适合只读取一个 Cookie。两条路径
    都不会因为前一字段已命中而忽略后续坏字段。
- `XHTTP_FEATURE_HTTP_SERVER_CONTENT_TYPE` 提供
  `xrtHttpServerRequestContentType`。缺失返回 `XHTTP_NEXT_END`，唯一有效字段返回
  `XHTTP_NEXT_ITEM`，重复或语法错误返回 `XHTTP_NEXT_ERROR`；结果借用请求快照。
- `XHTTP_FEATURE_HTTP_SERVER_FORM` 提供 `xrtHttpServerRequestForm`，只接受完整缓冲的
  `application/x-www-form-urlencoded` 正文并返回拥有型 `xqueryparams`。
- `XHTTP_FEATURE_HTTP_SERVER_FORM_DATA` 提供 `xrtHttpServerRequestFormData`，只接受
  完整缓冲的 `multipart/form-data` 正文并返回拥有型 `xformdata`；边界、字段和
  正文限制继续由公共 Multipart/FormData 配置控制。

两种 Form 便利函数不限制 HTTP 方法，因此 POST、PUT 和 PATCH 可以共享同一
解析路径。请求尚未完成时错误码为 `XHTTP_SERVER_REQUEST_ERROR_STATE`；流式或
丢弃正文没有可供拥有型解析器读取的副本，错误码为
`XHTTP_SERVER_REQUEST_ERROR_BODY`。Target、Header、Content-Type、Query 和 Form
失败均进入稳定错误域 `xrt.http.server.request`，并保留底层协议或内存 cause。

```c
xqueryparams* query = xrtHttpServerRequestQueryParams(
	request,
	NULL,
	NULL
  );
  xcookiepair session;
	xcookiepair cookies[16];
	size_t cookieCount;

if ( xrtHttpServerRequestCookie(
	request,
	XRT_STR_LITERAL("session"),
	&session
  ) == XCOOKIE_NEXT_ITEM ) {
  	/* session 借用 request；query 由调用方拥有。 */
  }
  if ( xrtHttpServerRequestCookies(
  	request,
  	cookies,
  	16,
  	&cookieCount
  ) ) {
  	/* 多键查询时只需扫描 cookies[0..cookieCount)。 */
  }
  xrtQueryParamsDestroy(query);
  ```

  所有请求解析输出都允许位于完整的未对齐存储。配置会先复制到局部快照，结果通过
  `memcpy` 发布；输出描述符和解码缓冲不得覆盖请求行、Header、正文或 Trailer，
  因此辅助器不会破坏仍由服务器和其他中间件共享的不可变请求事实。

完整组合示例位于 `examples/http/request_data/main.c`。每个便利功能均有独立
模块化、单头和裁剪依赖测试；聚合 OOM 测试逐次失败所有逻辑分配点，验证拥有型
结果和错误 cause 在失败时都能完整释放。

## Reply 构建器

启用 `XHTTP_FEATURE_HTTP_SERVER_REPLY` 需要 `HTTP_HEADERS` 与 `HTTP_BODY`，
但不依赖 TCP、TLS、线程、Future 或服务器对象。`xhttpreply` 拥有状态、
可选自定义原因短语、Header、Trailer 和一个独立正文引用。

空 Reply 只分配自身。标准原因短语直接借用静态协议表，Header、Trailer、
自定义原因和正文都在首次使用时创建，不存在旧版每对象固定字段数组或固定
正文缓冲。`xhttpreplyconfig` 可以分别约束 Header 与 Trailer；默认初始容量
为零，逻辑安全上限沿用动态 Header 的默认值。配置初始化和创建入口支持完整但
未对齐的描述符存储，并在返回前完成快照；调用方随后可以修改或释放原配置。
地址范围回绕、字段初始容量超过上限和容量运算溢出都会在分配 Reply 前失败。

```c
xhttpreply* reply = xrtHttpReplyCreate(XHTTP_STATUS_OK);

if ( (reply == NULL) ||
	!xrtHttpReplySetBytes(
		reply,
		(xbytesview){
			(cbytes)"{\"code\":200,\"msg\":\"OK\"}",
			23
		},
		XRT_STR_LITERAL("application/json; charset=utf-8")
	) ) {
	xrtHttpReplyDestroy(reply);
	return false;
}
```

`xrtHttpReplySetStatus` 同时恢复标准原因短语，`xrtHttpReplySetReason` 用于
显式覆盖，包括空原因短语。Header 与 Trailer 的 `Add` 保留重复项，`Set`
替换首项并折叠其余同名项；`EditHeaders` 和 `EditTrailers` 为需要批量操作的
底层用户暴露拥有型容器。协议准备层仍负责拒绝禁止的 Trailer、分帧冲突和
无正文状态，不在构建器中重复 HTTP/1 逻辑。

`xrtHttpReplySetBody` 保留一个 `xhttpbody` 引用，允许固定内存、引用内存、
文件或自定义生产者走同一条发送路径。`xrtHttpReplySetBytes` 是常用复制便利
入口，可同时设置 Content-Type，但不会提前生成 Content-Length 或
Transfer-Encoding。`Clone` 深复制可修改元数据并共享不可变正文来源。

`xrtHttp1ServerResponseInform` 是独立的信息响应入口，只接受 HTTP/1.1 的
`100..199`（不含由最终 Upgrade 路径处理的 `101`）。它允许 `103 Early Hints`
携带重复 Link 等普通 Header，但拒绝正文、Trailer、Content-Length、
Transfer-Encoding 和 Trailer 声明。完成输出只表示该条信息响应发送完毕，
当前请求仍必须继续到最终响应；`xrtHttp1ServerResponseInformational` 用于让
运行时保持这个边界。HTTP/1.0 不发送信息响应。

```c
xhttpreply* hints = xrtHttpReplyCreate(103);

xrtHttpReplyAddHeader(
	hints,
	XRT_STR_LITERAL("Link"),
	XRT_STR_LITERAL("</app.css>; rel=preload")
);
xhttp1serverresponse* information =
	xrtHttp1ServerResponseInform(XHTTP_VERSION_1_1, hints);
```

## HTTP/1 响应计划

启用 `XHTTP_FEATURE_HTTP_SERVER_RESPONSE` 需要服务端 Request、Reply 和
HTTP/1 Body。`xrtHttp1ServerResponseCreate` 从拥有型请求提取版本、方法和
连接事实；低层 `xrtHttp1ServerResponsePrepare` 直接接收这些事实，协议工具
无需为了写响应而构造请求对象。

准备过程冻结状态、原因、Header、Trailer 和正文引用。完整 Header 与最终
last-chunk 保存在一个按实际长度分配的紧凑块中；没有每连接发送缓冲，也不复制
正文。Reply 在准备成功后可以立即修改或销毁。

准备层唯一决定 HTTP/1 分帧：

- 已知正文长度生成规范 `Content-Length`，未知 HTTP/1.1 正文使用 chunked。
- 未知 HTTP/1.0 正文使用 close-delimited，并固定连接关闭事实。
- Trailer 自动生成 `Transfer-Encoding: chunked` 与 `Trailer` 声明。
- `HEAD` 和 `304` 只发送表示元数据，`204` 去除正文分帧，`205` 生成零长度。
- `101` 与成功 `CONNECT` 发送完 Header 后返回 `TUNNEL`，不解释后续字节。
- 其他 `1xx` 必须使用独立信息响应入口，不能误作最终响应提交。

`Output` 借出 Header、chunk 元数据或正文 Chunk，`OutputConsume` 按实际网络
短写推进；正文 Reader 的数据租约直到全部消费后才释放。已知长度来源提前结束
或越界、非法 Trailer、冲突分帧和正文来源错误都会进入稳定错误终态，错误域为
`xrt.http.server.response`。同步裁剪组合不会接受正文来源的 `AGAIN`；异步
正文由后续独立 Future 组合层提供等待能力。

`xrtHttp1ServerResponseOutput` 的输出描述符可以位于未对齐存储。函数先清空
有效输出，再发布借用线路片段；描述符不能覆盖 Response 状态、冻结 Header、
终止 chunk 或当前正文租约。非法输出不会消费线路字节，也不会破坏随后使用
合法描述符继续输出的能力。

```c
xhttp1serverresponse* response =
	xrtHttp1ServerResponsePrepare(
		XHTTP_VERSION_1_1,
		XRT_STR_LITERAL("GET"),
		XHTTP_SERVER_REQUEST_KEEP_ALIVE,
		reply
	);

while ( response != NULL ) {
	xbytesview data;
	xhttp1serveroutputstatus status =
		xrtHttp1ServerResponseOutput(response, 16384, &data);

	if ( status != XHTTP1_SERVER_OUTPUT_DATA ) {
		break;
	}
	/* 把实际发送长度传给 OutputConsume。 */
}
```

## 异步响应正文

异步正文被拆成两个可独立裁剪的层次：

- `XHTTP_FEATURE_HTTP_SERVER_RESPONSE_ASYNC` 依赖 `HTTP_SERVER_RESPONSE` 与
  `HTTP_BODY_ASYNC`，只为无 I/O 响应计划增加
  `xrtHttp1ServerResponseWait`。
- `XHTTP_FEATURE_HTTP_SERVER_BODY_ASYNC` 再依赖 `HTTP_SERVER`，负责把正文
  Future、Connection Worker、传输背压、写时限和关闭生命周期组合起来。

`xrtHttp1ServerResponseOutput` 返回 `XHTTP1_SERVER_OUTPUT_AGAIN` 后，重复调用
`Output` 会稳定返回 AGAIN，不会反复进入尚未就绪的正文源。调用方此时可以调用
`xrtHttp1ServerResponseWait` 取得一个自己拥有的 Future；只有 Future 成功完成后
才能再次调用 `Output`。在 AGAIN 之前、已有错误后或正文不支持 Wait 时调用会
失败。正文来源无法创建 Future 时，响应进入稳定的
`XHTTP1_SERVER_RESPONSE_ERROR_BODY` 终态，并保留来源错误链。

Server 组合层自动执行这条线路。Future 可以在任意线程完成，但响应状态机
只会回到 Connection 所属 Worker 推进。运行时使用 Connection 内嵌 waiter，
不会为每次等待额外创建 continuation 或任务。等待期间：

- `WriteTimeout` 继续计时，生产者永久不就绪不会永久占住连接。
- 传输层恢复有硬上限的读取，只缓存流水线字节而不解析下一请求，以便及时观察
  EOF、RST 和接收错误。
- 对端异常关闭、写超时、Connection abort 和 Server abort 都会请求取消 Future，
  摘除 waiter，并立即切断 Future 对 Connection 生命周期的持有。
- Future 失败时 Header 可能已经进入线路，运行时不会伪造第二条 500 响应，而是
  发布 `XHTTP_SERVER_ERROR_RESPONSE`，保留 Future 错误为 cause，并异常关闭连接。
- Future 取消只是协作通知；不合作的生产者可以继续运行，但不会继续持有
  Connection 或 Server。

正文 `Next` 仍必须遵守 `iMaxBytes`，DATA 的租约在全部传输短写完成后才释放。
已知长度正文的最终输出必须精确等于声明长度。完整可运行示例见
`examples/http/server_body_async/main.c`。

## 网络执行层

运行时、TLS/Future 和其他网络便利功能已迁入 [xhttp-server](../../../xhttp-server/docs/api/http_server.md)。完整迁移与所有权边界见 [分层说明](../design/http-layers.md)。
