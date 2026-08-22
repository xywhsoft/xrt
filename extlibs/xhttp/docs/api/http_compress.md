# HTTP Reply 自动压缩

`<xrt/http_compress.h>` 把内容编码协商、正文变换和服务端请求适配分成两层。
它只处理结构化 Reply；原始封包路径仍可直接写出完整 HTTP 数据，不依赖本模块。

## 两层入口

`xrtHttpReplyCompress` 接收已经解析的 `xhttpacceptencoding`、请求方法和 Reply。
它没有网络依赖，适合自定义服务器、测试、缓存和离线协议处理。

`xrtHttpServerReplyCompress` 读取服务端请求中的全部 `Accept-Encoding` 字段，
合并后调用同一个底层函数。请求正文尚未接收完成也可以调用。

两个入口都不修改输入 Reply。只有 `IDENTITY` 和 `APPLIED` 返回拥有型输出；
其他结果会把有效且不与输入重叠的输出槽置空，因此调用方可以统一清理：

- `XHTTP_REPLY_COMPRESS_SKIP`：响应不适用自动压缩，继续使用原 Reply。
- `XHTTP_REPLY_COMPRESS_IDENTITY`：返回带正确 `Vary` 的 identity 克隆。
- `XHTTP_REPLY_COMPRESS_APPLIED`：返回 gzip 或 deflate 克隆。
- `XHTTP_REPLY_COMPRESS_NOT_ACCEPTABLE`：没有客户端可接受的表示，调用方可返回 406。
- `XHTTP_REPLY_COMPRESS_ERROR`：配置、字段、内存或正文变换失败。

协议层错误使用 `http.reply.compress` 域和 `compress` operation；Server 便利层的
参数错误使用同一错误域和 `compress-server-reply` operation。输出槽自身无效或与
输入对象重叠时无法安全写入，函数只返回错误，不承诺改写该槽。
输出槽可以位于未对齐存储，实现通过字节复制一次性发布拥有型指针或空指针；
完整可写区间发生地址回绕时同样作为参数错误拒绝。

## 默认策略

默认提供 gzip 与 HTTP deflate，优先 gzip，最小正文为 1 KiB。固定正文不超过
64 KiB 时使用 `xrtDeflateAll` 一次性编码；若编码结果不小于 identity 且 identity
可接受，则返回 identity 克隆。更大的固定正文和流式正文组合
`xrtHttpBodyDeflate`，不会给 Reply 或连接预留固定传输缓冲。

流式路径默认每次最多从来源读取 32 KiB，并把尚未交付给调用方的压缩载荷限制为
1 MiB。`ReadSize` 只控制推进粒度，`QueueLimit` 是内部队列硬上限，零表示显式取消
限制；已经交付且仍由调用方持有的 Chunk 不计入该队列。`OutputLimit` 则限制整个
Deflate 编码过程的累计输出，三者互不替代。一次性 eager 路径不建立流式队列，
但仍遵守 `OutputLimit`。

为兼容未正确实现内容编码的旧客户端，`Accept-Encoding` 缺失时默认选择
identity。设置 `XHTTP_REPLY_COMPRESS_ALLOW_ABSENT` 可采用 RFC 允许的任意编码。

默认不压缩未知长度、不可压缩 MIME、范围响应、已经编码的响应和带
`Cache-Control: no-transform` 的响应。配置标志可以显式放开未知长度、任意
媒体类型和 `no-transform` 策略。

## 元数据

自动选择可能随请求变化时，输出会保留或增加 `Vary: Accept-Encoding`。
该路径复用独立的 [`http_vary.md`](http_vary.md) 公共协议层。任一重复字段中
已有 `Vary: *` 时不会追加字段；星号与名称混合仍按星号主导，不误报协议错误。
空 `Vary` 保留为空字段事实，但不表示已经声明 `Accept-Encoding`，因此输出会
另加一个 `Vary: Accept-Encoding`。全部重复字段都会严格验证，合法前缀不能掩盖
畸形后缀。

应用内容编码后，模块会：

- 设置 `Content-Encoding`；
- 删除旧 `Content-Length` 与 `Transfer-Encoding`，让协议准备层重新分帧；
- 删除 `Content-MD5`、`Digest`、`Content-Digest` 与 `Repr-Digest`；
- 删除 `Accept-Ranges` 和旧 `Trailer` 声明；
- 从 Header 与 Trailer 删除已经失效的摘要；
- 只保留语法正确的弱 ETag，删除强、重复或畸形 ETag；
- 保留与编码无关的 Content-Type、Content-Language、Last-Modified 和自定义 Trailer。

## 示例

```c
xhttpacceptencoding accept;
xhttpreplycompressconfig config;
xhttpreply* selected = NULL;
xhttpreplycompressstatus status;

xrtHttpAcceptEncodingInit(&accept);
xrtHttpAcceptEncodingAdd(
	&accept,
	XRT_STR_LITERAL("gzip, identity;q=0.5")
);
xrtHttpReplyCompressConfigInit(&config);

status = xrtHttpReplyCompress(
	&accept,
	XRT_STR_LITERAL("GET"),
	reply,
	&config,
	&selected
);

if ( status == XHTTP_REPLY_COMPRESS_NOT_ACCEPTABLE ) {
	/* 返回 406。 */
} else if ( status == XHTTP_REPLY_COMPRESS_SKIP ) {
	/* 发送原 reply。 */
} else if ( status >= XHTTP_REPLY_COMPRESS_IDENTITY ) {
	/* 发送 selected，结束后销毁。 */
}
```

非可重放流式正文不会在变换阶段打开或消费。输出 Reply 和原 Reply 共享同一个
一次性来源，因此调用方必须二选一发送，不能把两者都打开。
