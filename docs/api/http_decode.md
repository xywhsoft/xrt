# HTTP 正文解码

`<xrt/http_decode.h>` 把零分配的 `Content-Encoding` 解析与通用 Inflate 组合为
独立、可裁剪的流式正文解码器。它不依赖 HTTP 客户端、服务器、Body 对象或网络
缓冲区，可以直接接在 `xrtHttp1BodyRead` 产生的数据片段之后。

## 模式

- `XHTTP_DECODE_IDENTITY`：没有内容编码或只有 `identity`，输入视图直接交给回调；
- `XHTTP_DECODE_CONTENT`：按线路声明的逆序执行 gzip 或 HTTP deflate 解码；
- `XHTTP_DECODE_RAW`：仅在显式设置 `XHTTP_DECODE_ALLOW_RAW` 后，对未知编码原样交付。

默认策略严格拒绝未知编码，避免调用方把未解码字节误当作明文。代理、缓存或需要
保留扩展编码的程序可以选择原样回退，并通过 `xrtHttpDecodeMode` 保留元数据。

## 限额

`OutputLimit` 同时限制每个中间解码层和最终明文，防止嵌套压缩绕过膨胀限制。
`GzipHeaderLimit` 限制可选 gzip Header，`MaxCodings` 限制嵌套层数。默认最多四层，
实现硬上限为 `XHTTP_CONTENT_CODINGS_MAX`。

解码器不保存正文。输出视图只在回调期间有效，回调必须在返回前消费或复制数据。
`xrtHttpDecodeReset` 会复用已经分配的 Inflate 滑动窗口，适合连接池和长连接逐消息
处理，不需要为每个响应重新分配 32 KiB 窗口。

输出回调返回 `false` 会把当前解码器置为失败终态；本次输入与输出计数不会发布，后续
写入返回状态错误。调用方处理完自己的回调错误后，可以显式调用 `xrtHttpDecodeReset`
开始下一条消息。字段描述符和配置允许未对齐存储，多层创建的每个 OOM 点都保证释放
已经建立的解码层。

```c
xhttpdecode* pDecode = xrtHttpDecodeCreate(
	pHead->Fields,
	pHead->FieldCount,
	NULL
);

xrtHttpDecodeWrite(
	pDecode,
	BodyData,
	bBodyDone,
	onBody,
	pContext
);
```

HTTP/1 的报文边界仍由 `xrtHttp1BodyRead` 决定。只有该 reader 返回最终完成状态时，
最后一次 `xrtHttpDecodeWrite` 才传入 `bFinal = true`；解码器随后会验证压缩流结束、
gzip CRC 与长度 trailer。
