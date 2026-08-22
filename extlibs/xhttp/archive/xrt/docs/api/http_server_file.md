# HTTP 服务端文件响应

`http_server_file` 把异步文件 Body、无网络 Reply Builder 和 HTTP Server Future 桥组合成一条可裁剪链路。它不重新实现文件读取，也不把静态网站语义塞进 HTTP 传输核心。

## 裁剪与分层

启用 `XRT_FEATURE_HTTP_SERVER_FILE` 会引入：

- `XRT_FEATURE_HTTP_BODY_FILE`：异步文件正文与严格区间。
- `XRT_FEATURE_HTTP_SERVER_FUTURE`：把 Reply Future 绑定到 Connection Worker。
- `XRT_FEATURE_HTTP_SERVER_BODY_ASYNC`：在 TCP 背压和写时限下等待异步正文。

底层用户可以直接使用 `xrtHttpBodyFileFuture` 并自行构造 Reply；需要先准备 Reply、增加字段或组合错误处理时使用 `xrtHttpReplyFileFuture`；常见服务端路径使用 `xrtHttpConnFile` 一次完成。

## Reply Future

```c
xfuture* pReply = xrtHttpReplyFileFuture(
	pFiles,
	XHTTP_STATUS_OK,
	XRT_STR_LITERAL("application/octet-stream"),
	"asset.bin"
);
```

成功 Future 拥有一个 `xhttpreply*`。Reply 引用文件 Body，Future 最后释放时会销毁 Reply，并最终异步关闭文件。文件打开、大小查询和区间检查都在调用方提供的有界任务池执行。

Reply Future 独占对应的文件准备 Future。对 Reply Future 请求取消时，组合层会把协作取消向上游传播；尚未开始的任务被工作线程取出后直接发布取消终态，不会打开文件，任务池队列满时也会主动清除已取消槽位。该行为只属于明确独占生产链的文件 Helper，不会改变普通 `xrtFutureThen` 对共享源的单向观察语义。

`xrtHttpReplyFileRangeFuture` 使用严格的 `[offset, offset + length)` 区间。区间越过准备时的文件大小会失败，不会静默缩短。

路径和非空 Content-Type 都在函数返回前复制，调用方随后可以释放或修改原存储。Content-Type 为空时不增加该字段。Content-Length 由 HTTP/1 响应准备层根据已知 Body 长度生成，Helper 不手工拼接协议字段。

Reply Future Builder 不接触网络，可以从任意线程调用。参数错误、同步分配失败或任务提交失败直接返回 `NULL`，且不会留下半成品 Reply 或文件任务；受理后发生的打开、大小或区间错误通过 Future 失败结果发布，错误域保持为 `http.body.file`。

## 连接 Helper

```c
if ( !xrtHttpConnFile(
	pConnection,
	pFiles,
	XHTTP_STATUS_OK,
	XRT_STR_LITERAL("application/json; charset=utf-8"),
	"response.json"
) ) {
	(void)xrtHttpConnReply(
		pConnection,
		XHTTP_STATUS_SERVICE_UNAVAILABLE,
		XRT_STR_LITERAL("text/plain; charset=utf-8"),
		XRT_BYTES_LITERAL("Service Unavailable")
	);
}
```

`xrtHttpConnFile` 和 `xrtHttpConnFileRange` 只能在 Connection Worker 回调内调用。返回 `true` 表示最终响应 Future 已经受理，不表示文件已经打开或字节已经发送。连接先关闭时，Connection Future 桥会取消 Reply Future，Reply Future 再取消独占的文件准备 Future；析构链随后回收 Reply、Body、异步文件和路径副本。

任务池必须存活到已受理的文件准备、Body 和异步关闭全部离开。可以在停止新请求后调用 `xrtTaskPoolClose`，内部资源 finalizer 仍会继续执行；只有相关响应 Future 和 Connection 全部结束后才能调用 `xrtTaskPoolDestroy`。

`xrtHttpConnFile` 即时返回 `false` 时，当前请求仍未绑定最终响应，可以直接提交固定错误响应。返回 `true` 后，最终响应槽已经交给 Future 桥；后续文件错误按通用 Future 响应契约映射并发布，调用方不能再提交第二条最终响应。

## 明确边界

本模块只处理“把指定文件或区间作为响应正文”：

- 不自动调用 `http_semantics` 解析 Range 或 If-Range。
- 不自动生成 Content-Range、ETag、Last-Modified、Accept-Ranges。
- 不推断 MIME 类型。
- 不把文件缺失自动映射为 404，也不把无效区间自动映射为 416。

Range、Content-Range、ETag 和条件请求的公共原语已经由
`XRT_FEATURE_HTTP_RANGE`、`XRT_FEATURE_HTTP_ETAG` 与
`XRT_FEATURE_HTTP_PRECONDITION` 提供；本模块刻意不强制组合它们。直接使用
本模块时，调用方可以通过更底层的 Body Future 自行映射错误和设置任意字段，
不会被固定流程限制。自动路径映射、缓存策略和状态响应仍属于后续独立的
`http_static` 层。

完整示例见 `examples/http/server_file/main.c`。

## 旧版资产与后续承接

旧版 `dev/ver1/lib/xhttpd.h` 的 `xrtHttpdConnSendFile` 和
`xrtHttpdConnSendFileRange` 已经证明完整文件、区间、分块读取和统一响应发送是
必要能力。新版保留这些场景，但用唯一的异步文件 Body 和服务器响应泵承接，
不再在 Connection Worker 中同步打开、读取和关闭文件，也不为每次发送固定
分配 64 KiB chunk。严格区间替换了旧版越界后静默缩短的行为。

旧版 `dev/ver1/lib/xweb.h` 的静态资源实现及
`dev/ver1/test/test_xweb.h` 的端到端测试继续按层迁移。已经完成：

- ETag 解析、强弱比较、列表与写出。
- If-Match、If-Unmodified-Since、If-None-Match、If-Modified-Since 的固定顺序。
- If-Range 强验证器判断。
- 固定、开放尾、suffix 和多 byte-range，以及 Content-Range。
- 64 位线路整数、溢出、零长度和 206/304/412/416 所需结果分类。

后续 `http_static` 仍需承接：

- GET/HEAD、挂载点、索引文件、MIME 和 Cache-Control。
- 路径 percent 解码、`.`、`..`、反斜线和隐藏文件策略。
- 文件身份稳定的验证器生成、同句柄元数据与打开后路径约束。
- 多范围合并、multipart/byteranges 正文和状态响应组装。

旧代码不能直接复制：它先按路径查询再打开文件，存在 TOCTOU；路径文本检查
不能阻止符号链接逃出根目录；仅按大小和修改时间生成的值却使用强 ETag 外形；
因此旧的路径与验证器生成仍不能直接复制。协议向量已经迁入
`tests/http/test_http_etag.c`、`test_http_precondition.c` 和
`test_http_range.c`；后续静态层继续增加符号链接、文件替换、多范围响应和
同句柄 TOCTOU 覆盖。
