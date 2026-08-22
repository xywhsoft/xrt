# HTTP 流式压缩正文

`http_body_deflate.h` 在通用 `xhttpbody` 与 Deflate 编码器之间提供独立变换层。它不依赖
HTTP 客户端、服务器、Reply、文件或网络，因此请求正文、响应正文、文件正文和自定义
正文都能复用同一实现。协议协商与 Header 修改属于更高一层，不由本模块暗中完成。

## 裁剪与依赖

- **XRT_FEATURE_HTTP_BODY_DEFLATE**：依赖 **XRT_FEATURE_HTTP_BODY_TRANSFORM**、
  **XRT_FEATURE_HTTP_BODY** 和 **XRT_FEATURE_DEFLATE**。
- 异步 Body 同时启用 **XRT_FEATURE_HTTP_BODY_ASYNC** 时，变换 Reader 会透明转发
  来源的 `AGAIN` 与 `Wait`；模块本身不创建线程或调度器。
- 不启用本模块时，普通 Body、Deflate、HTTP 客户端和服务器都不会带入变换代码。

## 配置

~~~c
#define XHTTP_BODY_DEFLATE_READ_DEFAULT 32768u
#define XHTTP_BODY_DEFLATE_QUEUE_DEFAULT 1048576u

typedef struct xhttpbodydeflateconfig {
	xdeflateconfig Deflate;
	size_t ReadSize;
	size_t QueueLimit;
} xhttpbodydeflateconfig;

XRT_API void xrtHttpBodyDeflateConfigInit(
	xhttpbodydeflateconfig* pConfig);
~~~

默认输出确定性 gzip，级别为 6，策略为默认，编码总量没有额外上限。`ReadSize` 是每次向
来源 Reader 请求的最大字节数，只控制状态机推进粒度，不会在 Body 或 Reader 中预留
同等大小的固定缓冲。生产服务可以用 `Deflate.OutputLimit` 给未知或不可信来源设置编码
总量硬上限。

配置初始化输出和构造输入都可以未对齐，但必须是完整且不回绕的内存区间。构造函数在返回前逐字节复制完整配置，调用方随后可以立即修改或释放原结构；`ReadSize == 0` 会同步拒绝。

`QueueLimit` 限制一次推进期间 Reader 内部尚未交付的编码载荷，默认 1 MiB，设为 `0`
表示不限制。超过限制会形成稳定范围错误，而不是继续增长内存。已经交给调用方的 Chunk
不再计入内部队列，因为其释放时机由调用方控制；需要控制这部分内存时，调用方应及时
释放 Chunk，并把并行发送租约纳入自己的背压预算。

## 创建与所有权

~~~c
XRT_API xhttpbody* xrtHttpBodyDeflate(
	xhttpbody* pSource,
	const xhttpbodydeflateconfig* pConfig);
~~~

成功后结果持有 `Source` 的独立引用，调用方可以立即销毁自己的来源引用。结果长度始终为
`XHTTP_BODY_UNKNOWN`，因为压缩长度只能在编码完成后确定。结果只有在来源可重放时才可
重放；每次 `Open` 都创建独立来源 Reader 和编码器，因此并发重放不会共享可变状态。

输出按编码器实际产生的片段分块拥有，不存在每对象 8 KiB 或 32 KiB 固定缓冲。发布的
Chunk 持有输出块引用，因此可以晚于变换 Reader 和 Body 释放。尚未交付的输出受
`QueueLimit` 约束，并在 Reader 关闭时回收。

来源错误、压缩限额、OOM 和编码错误不会转换成模糊布尔值。Body Reader 会稳定保存原始
结构化错误；来源返回 `AGAIN` 时，`xrtHttpBodyReaderWait` 直接组合来源 Future。

## 示例

~~~c
xhttpbody* Source = xrtHttpBodyBorrow(
	XRT_BYTES_LITERAL("response payload"));
xhttpbody* Gzip = xrtHttpBodyDeflate(Source, NULL);

xrtHttpBodyDestroy(Source);
if ( Gzip == NULL ) {
	/* 处理配置或内存错误。 */
}
~~~

完整范例位于 `examples/http/body_deflate/main.c`。

## 验证

- raw DEFLATE、zlib、gzip 与一次性 Deflate 输出逐字节一致。
- 空正文、文本、高熵数据、1 字节来源推进和变化 Chunk 上限均通过 Inflate 闭环。
- 来源引用、重放能力、总输出和队列硬上限、来源错误及非可重放二次打开边界已覆盖。
- `AGAIN/Wait`、来源等待错误和 Future 脱离 Reader 的生命周期使用独立异步测试。
- Chunk 晚于 Reader/Body 销毁释放，防止异步发送路径出现悬挂引用。
- 单头、独立裁剪、依赖缺失、OOM 和多编译器由模块发布门禁验证。
