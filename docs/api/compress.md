# 压缩与解压缩

`<xrt/compress.h>` 提供与网络和 HTTP 解耦的压缩数据流能力。
`XRT_FEATURE_INFLATE` 包含原始 DEFLATE、zlib、HTTP 兼容 `deflate`
和 gzip 解码；`XRT_FEATURE_DEFLATE` 独立提供 raw、zlib 和 gzip 编码。
两个方向可以分别裁剪，HTTP 与 WebSocket 只在需要时组合它们。

## 对象与内存

`xrtInflateCreate` 只在真正需要解码时分配一个对象。对象包含 DEFLATE
算法必需的 32 KiB 滑动窗口；普通网络连接和未压缩 HTTP 调用不会承担这份
内存。`WindowBits` 接受 8 到 15，并严格拒绝超过配置窗口的回溯距离，而不只是
作为内存提示。`xrtInflateReset` 保留窗口并复位状态，适合反复处理独立数据流。

输出由 `xinflateoutputproc` 同步接收，视图只在回调期间有效。回调为空时
执行完整验证但丢弃输出。`xrtInflateAll` 是常见整块场景的便捷函数，返回值
由 `xrtFree` 释放并额外带一个不计入长度的零字节。

`xrtDeflateCreate` 同样按需分配算法状态。编码器内部包含 DEFLATE 的 32 KiB
历史字典、匹配表和 Huffman 工作区，因此不能按连接或请求提前常驻创建；
HTTP 应只在协商选中压缩后创建，WebSocket 只有启用上下文接管时才长期复用。
XRT 默认使用约 164 KiB 的低内存编码布局，而不是约 312 KiB 的普通布局；
`xrtDeflateReset` 保留这块状态并开始独立的新数据流。

两个方向的配置初始化、Create 和 Reset 都只要求配置位于有效的连续存储中，
不要求调用方保证结构体自然对齐。实现通过 `memcpy` 建立本地配置快照，不会在
对象生命周期内引用调用方配置；因此栈内配置、封包内配置和临时配置都遵循同一契约。

## 格式

- `XINFLATE_RAW`：没有包装的 RFC 1951 DEFLATE。
- `XINFLATE_ZLIB`：带 RFC 1950 Header 和 Adler-32 的 zlib。
- `XINFLATE_DEFLATE`：先检查 zlib Header，否则按 raw 解码，用于兼容实际
  HTTP `Content-Encoding: deflate` 服务。
- `XINFLATE_GZIP`：校验 Header、可选 Header CRC、正文 CRC32 和 ISIZE，
  并接受规范的拼接 gzip member。

Deflate 的 `RAW` 与 `ZLIB` 生成对应标准数据流；`GZIP` 使用 `MTIME=0`、
无可选字段和 `OS=255` 的确定性 Header，并生成 CRC32 与 ISIZE trailer。
`WindowBits` 同样接受 8 到 15，限制编码器可生成的最大回溯距离；zlib Header
中的 CINFO 会同步反映该窗口。相同输入、配置和 Flush 序列产生稳定输出，不启用
依赖未初始化内存的快速模式。

## 编码与 Flush

```c
xdeflateconfig Config;
xdeflate* pDeflate;

xrtDeflateConfigInit(&Config);
Config.Format = XDEFLATE_GZIP;
Config.Level = 6;
Config.WindowBits = 15;
Config.OutputLimit = 8u * 1024u * 1024u;

pDeflate = xrtDeflateCreate(&Config);
xrtDeflateWrite(
	pDeflate,
	First,
	XDEFLATE_FLUSH_NONE,
	onCompressed,
	pOutput
);
xrtDeflateWrite(
	pDeflate,
	Last,
	XDEFLATE_FLUSH_FINISH,
	onCompressed,
	pOutput
);
xrtDeflateDestroy(pDeflate);
```

- `NONE`：只推进输入，不强制输出边界。
- `SYNC`：输出可同步解码的空块并保留历史字典，适合
  WebSocket `permessage-deflate` 消息边界。
- `FULL`：建立同步边界并清空后续匹配历史。
- `FINISH`：结束唯一数据流；成功后只能查询或 Reset。

输出回调同步执行。回调内对同一对象的 Reset、再次 Write 或 Destroy 会被
`XERR_STATE` 拒绝，只读状态查询仍然安全；同一对象也不能被多个线程并发操作。
回调返回 `false` 可保留自己设置的结构化错误；没有错误时 Deflate 生成
`XERR_CANCELLED / XDEFLATE_ERROR_OUTPUT`。`OutputLimit` 计算全部线路字节，
包括 zlib Header/Adler-32 或 gzip Header/trailer。Inflate 使用相同的回调
重入规则。

## 安全边界

`OutputLimit` 是解码后总字节硬上限，可阻止压缩炸弹无限扩张。
`GzipHeaderLimit` 分别限制每个 gzip member 的 Header，默认 64 KiB。
默认 Inflate 和 Deflate 窗口都是 15 位；较小窗口必须由两端显式配置。解码器同时
校验 zlib CINFO 和每个实际匹配距离，因此协议层可以把协商后的窗口作为安全边界。
损坏校验、截断、尾随数据、超过限额和输出回调中止都会使对象进入失败终态；
调用 `xrtInflateReset` 后才能复用。

所有公开输入视图和输出长度槽都会先校验完整地址范围，包括整数地址回绕。
无效参数在读取输入和改变算法状态前同步失败，因此无效 `Write` 后对象仍可继续使用。
`xrtInflateAll` 与 `xrtDeflateAll` 只在完整成功后写入输出长度；参数、配置、分配、
编解码或消费者失败都保留调用前的长度值。输出长度槽也只要求有效连续存储，
不要求自然对齐。

Deflate 的配置错误、输出限额、消费者拒绝或内部编码异常同样进入失败终态；
调用 `xrtDeflateReset` 可以保留算法内存并重新开始。`xrtDeflateAll` 返回
`xrtFree` 释放的连续结果，并且只在成功时修改输出长度。

```c
xinflateconfig Config;
bytes pData;
size_t iSize;

xrtInflateConfigInit(&Config);
Config.Format = XINFLATE_GZIP;
Config.OutputLimit = 8u * 1024u * 1024u;

pData = xrtInflateAll(Input, &Config, &iSize);
if ( pData == NULL ) {
	const xerror* pError = xrtGetError();
	/* 处理 xrt.inflate 错误。 */
}
xrtFree(pData);
```

一次性编码对应：

```c
xdeflateconfig Config;
bytes pGzip;
size_t iSize;

xrtDeflateConfigInit(&Config);
pGzip = xrtDeflateAll(Input, &Config, &iSize);
if ( pGzip == NULL ) {
	const xerror* pError = xrtGetError();
	/* 处理 xrt.deflate 错误。 */
}
xrtFree(pGzip);
```

完整示例见 `examples/compress/inflate/main.c` 和
`examples/compress/deflate/main.c`。
