# SSE 服务器

编码、解析器与 HTTP 内容描述在 [xhttp](../../../xhttp/docs/api/http_sse.md)。

## EventSource 服务端

`http_sse_server` 不建立 SSE 专用连接、线程、队列或网络状态机。它创建一个状态
`200`、规范 `Content-Type: text/event-stream` 和未知长度一次性 Body 的普通
`xhttpreply`，实际发送仍由 HTTP Server 的唯一响应状态机、TCP/TLS 背压和写时限驱动：

```c
xhttpreply* xrtHttpSseReplyCreate(
	const xhttpbodystreamconfig* pConfig,
	xhttpbodystream** ppStream
);
xhttpbodystreamresult xrtHttpSseSend(
	xhttpbodystream* pStream,
	xstrview Data
);
xhttpbodystreamresult xrtHttpSseSendEvent(
	xhttpbodystream* pStream,
	const xhttpsseevent* pEvent
);
xhttpbodystreamresult xrtHttpSseSendComment(
	xhttpbodystream* pStream,
	xstrview Comment
);
```

`ReplyCreate` 只设置协议必需的 Content-Type。`Cache-Control`、认证、CORS、
`X-Accel-Buffering` 和代理空闲策略属于应用或部署环境，调用方可以在提交 Reply 前
通过普通 Header API 设置。提交给 `xrtHttpConnRespond` 后，Server 已经冻结并保留
正文来源，Reply 可以销毁；返回的 Stream 是独立生产端，可以交给后台任务或其他
线程继续写入。

配置在调用期间完成快照；配置和生产端输出都允许位于完整但未对齐的存储中，
二者不得重叠。失败不会发布半初始化的生产端。

```c
xhttpbodystream* stream = NULL;
xhttpreply* reply = xrtHttpSseReplyCreate(NULL, &stream);

if ( (reply == NULL) ||
	!xrtHttpReplySetHeader(
		reply,
		XRT_STR_LITERAL("Cache-Control"),
		XRT_STR_LITERAL("no-cache")
	) ||
	(xrtHttpConnRespond(connection, reply) != XNET_RESULT_OK) ) {
	xrtHttpBodyStreamDestroy(stream);
	stream = NULL;
}
xrtHttpReplyDestroy(reply);

/* 成功后由应用保存 stream，并在任务或发布器中写入。 */
```

每次 `SendEvent`、`Send` 和 `SendComment` 都先完整验证并计量，再直接编码到一个
Body Stream 节点；不会建立临时事件字符串，也不会在失败时提交半条事件。Stream
的字节与 Chunk 硬预算覆盖并发预留、排队和活动租约。返回 `AGAIN` 时没有保留任何
输入引用，调用方可以等待 `xrtHttpBodyStreamWaitWritable` 后重试；共享 Future 只表示
下一代可写性，单个等待者取消它会影响其他等待者，因此不应直接取消。

最后一个 Stream 引用销毁会在已排队事件之后发布正常 EOF；`Close` 可以提前幂等
关闭输入，`Fail` 会丢弃尚未交付的事件并把稳定 Cause 传给 HTTP Server。消费者或
连接先关闭后，后续写入返回 `CLOSED`。需要预编码固定事件、代理字节或自定义扩展时，
可以直接使用 `xrtHttpBodyStreamWrite`、`WriteRef` 或 `WriteTake`，结构化 Writer 不是
强制路径。
