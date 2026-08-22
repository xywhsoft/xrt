# HTTP 流式解压正文

`http_body_inflate.h` 在通用 `xhttpbody` 与 Inflate 解码器之间提供独立变换层。
它不依赖 HTTP 客户端、服务器、Reply、文件或网络，因此响应正文、请求正文、文件正文
和自定义正文都能使用同一套流式解压契约。单层 API 只解码调用方明确选择的一层；
可选的通用 Body 解码层组合协议计划与多层 Inflate，但不修改 Header。

## 裁剪与依赖

- **XRT_FEATURE_HTTP_BODY_INFLATE**：依赖 **XRT_FEATURE_HTTP_BODY_TRANSFORM**、
  **XRT_FEATURE_HTTP_BODY** 和 **XRT_FEATURE_INFLATE**。
- **XRT_FEATURE_HTTP_BODY_DECODE**：额外依赖 **XRT_FEATURE_HTTP_ENCODING**，
  提供 Content-Encoding 到多层 Body 的便利组合；不用时可以单独裁掉。
- 异步 Body 同时启用 **XRT_FEATURE_HTTP_BODY_ASYNC** 时，变换 Reader 会透明转发
  来源的 `AGAIN` 与 `Wait`；模块本身不创建线程或调度器。
- 不启用本模块时，普通 Body、Inflate、HTTP 客户端和服务器都不会带入正文变换代码。

## 配置

~~~c
#define XHTTP_BODY_INFLATE_READ_DEFAULT 32768u
#define XHTTP_BODY_INFLATE_OUTPUT_DEFAULT UINT64_C(67108864)
#define XHTTP_BODY_INFLATE_QUEUE_DEFAULT 67108864u

typedef struct xhttpbodyinflateconfig {
	xinflateconfig Inflate;
	size_t ReadSize;
	size_t QueueLimit;
} xhttpbodyinflateconfig;

XRT_API void xrtHttpBodyInflateConfigInit(
	xhttpbodyinflateconfig* pConfig);
~~~

默认使用 `XINFLATE_DEFLATE`，同时兼容 HTTP 历史上出现的 zlib 包装和 raw DEFLATE，
并把单个正文的解码总量限制为 64 MiB。明确的 `gzip`、`zlib` 或 `raw` 表示应设置对应
`Inflate.Format`。`GzipHeaderLimit` 继续限制每个 gzip member 的 Header，gzip 拼接
member 会按顺序形成一个明文正文。

`ReadSize` 只控制每次向来源 Reader 请求的最大线路字节数，不会在 Body 或 Reader 中
预留同等大小的传输缓冲。每个已打开 Reader 按需拥有一个 Inflate 解码器；其中约
32 KiB 滑动窗口是 DEFLATE 算法状态，不是每连接预分配的网络缓冲。

`QueueLimit` 限制一次同步解码推进中尚未交付的明文载荷，默认 64 MiB，设为 `0` 表示
不限制。高压缩比输入可能在一次 `InflateWrite` 中产生大量明文，因此该限制与总量
`Inflate.OutputLimit` 必须分别存在：前者约束 Reader 的瞬时内部队列，后者约束整个
解码结果。超过任一限制都会形成稳定范围错误，不会继续增长内存。

不可信数据必须保留有限的 `Inflate.OutputLimit` 和 `QueueLimit`。需要处理更大可信正文
时可以显式提高对应上限；库不会根据线路长度推测安全的解码长度。已经交给调用方的
Chunk 不计入内部队列，其并行持有量应由调用方自己的发送或处理背压约束。

## 创建与所有权

~~~c
XRT_API xhttpbody* xrtHttpBodyInflate(
	xhttpbody* pSource,
	const xhttpbodyinflateconfig* pConfig);
~~~

成功后结果持有 `Source` 的独立引用，调用方可以立即销毁自己的来源引用。结果长度始终
为 `XHTTP_BODY_UNKNOWN`，只有完整解码后才能知道明文长度。结果只有在来源可重放时才
可重放；每次 `Open` 都创建独立来源 Reader 和解码器，因此并发重放不共享可变状态。

输出按解码器实际产生的片段拥有，不存在每对象固定 8 KiB 或 32 KiB 传输缓冲。发布的
Chunk 持有输出块引用，可以晚于变换 Reader 和 Body 释放。尚未交付的输出受
`QueueLimit` 约束，并在 Reader 关闭时回收。

截断流、非法数据、gzip Header/CRC/长度错误、输出上限、OOM 和来源错误都会成为稳定
Reader 错误。来源返回 `AGAIN` 时，`xrtHttpBodyReaderWait` 直接组合来源 Future。

## 多层 Content-Encoding

一个 `xrtHttpBodyInflate` 只解码调用方明确选择的一层。协议驱动的重复字段解析、逆序
组合、未知编码回退和层数预算位于独立的
[`http_body_decode.h`](http_body_decode.md)，不用时不会进入本模块的头文件或依赖闭包。

## 示例

单层范例位于 `examples/http/body_inflate/main.c`；多层协议组合范例和完整契约见
[`http_body_decode.md`](http_body_decode.md)。

## 验证

- raw DEFLATE、zlib、HTTP deflate 双格式识别、gzip 和拼接 gzip member。
- 空正文、高熵大正文、1 字节线路推进和变化 Chunk 上限的 Deflate 闭环。
- 截断、CRC 损坏、解压总量与队列上限、来源错误和稳定 Reader 终态。
- `AGAIN/Wait`、等待错误、算法窗口 OOM、输出块 OOM 和 16 路并发重放。
- Chunk 晚于 Reader/Body 销毁释放，以及单头、裁剪和依赖缺失门禁。
