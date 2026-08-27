#ifndef XRT_HTTP_DECODE_H
#define XRT_HTTP_DECODE_H

#include <xrt/http_encoding.h>



#if defined(XRT_FEATURE_HTTP_DECODE) && \
	(!defined(XRT_FEATURE_HTTP_ENCODING) || \
	 !defined(XRT_FEATURE_INFLATE))
	#error "XRT HTTP decode requires Content-Encoding and Inflate support"
#endif



#if defined(XRT_FEATURE_HTTP_DECODE)

#define XHTTP_DECODE_OUTPUT_UNLIMITED UINT64_MAX
#define XHTTP_DECODE_OUTPUT_SAFE_DEFAULT (UINT64_C(16) * 1024u * 1024u)



/* 解码模式明确区分无编码、成功接管的内置编码和显式允许的原样回退。 */
typedef enum xhttpdecodemode {
	XHTTP_DECODE_IDENTITY = 0,
	XHTTP_DECODE_CONTENT,
	XHTTP_DECODE_RAW
} xhttpdecodemode;



/* 默认拒绝未知编码；调用方可显式选择保留整个原始表示。 */
typedef enum xhttpdecodeflag {
	XHTTP_DECODE_ALLOW_RAW = UINT32_C(0x00000001)
} xhttpdecodeflag;



/* 错误码覆盖配置、Content-Encoding、状态和输出边界。 */
typedef enum xhttpdecodeerror {
	XHTTP_DECODE_ERROR_ARGUMENT = 1,
	XHTTP_DECODE_ERROR_CONFIG,
	XHTTP_DECODE_ERROR_CONTENT_ENCODING,
	XHTTP_DECODE_ERROR_UNSUPPORTED,
	XHTTP_DECODE_ERROR_STATE,
	XHTTP_DECODE_ERROR_LIMIT,
	XHTTP_DECODE_ERROR_OUTPUT
} xhttpdecodeerror;



/* 每个解码层和最终明文都受同一个硬限额约束。 */
typedef struct xhttpdecodeconfig {
	uint64 OutputLimit;
	uint32 GzipHeaderLimit;
	uint32 MaxCodings;
	uint32 Flags;
} xhttpdecodeconfig;



/* HTTP 解码器拥有并复用底层 Inflate 状态。 */
typedef struct xhttpdecode xhttpdecode;



/* 输出视图只在回调期间有效，返回 false 会终止当前解码器。 */
typedef bool (*xhttpdecodeoutputproc)(xbytesview Data, ptr pData);



XRT_EXTERN_C_BEGIN



/* 初始化兼容配置：最多四层、64 KiB gzip Header、明文长度不设上限。 */
XRT_API void xrtHttpDecodeConfigInit(xhttpdecodeconfig* pConfig);



/*
	初始化面向不可信对端的安全配置；除协议限制外，明文最多 16 MiB。
	需要更大正文时应显式修改 OutputLimit，使用无限制必须显式设为
	XHTTP_DECODE_OUTPUT_UNLIMITED。
*/
XRT_API void xrtHttpDecodeConfigInitSafe(xhttpdecodeconfig* pConfig);



/* 根据全部 Header 创建解码器；字段和值只在本次调用期间借用。 */
XRT_API xhttpdecode* xrtHttpDecodeCreate(
	const xhttpfield* pFields,
	size_t iCount,
	const xhttpdecodeconfig* pConfig
);



/* 为下一条消息复位并复用已经分配的 Inflate 窗口。 */
XRT_API bool xrtHttpDecodeReset(
	xhttpdecode* pDecode,
	const xhttpfield* pFields,
	size_t iCount,
	const xhttpdecodeconfig* pConfig
);



/*
	同步消费完整输入片段；bFinal 表示 HTTP 正文已经到达协议边界。
	无编码和原样回退路径直接调用 Output，不复制输入。
*/
XRT_API bool xrtHttpDecodeWrite(
	xhttpdecode* pDecode,
	xbytesview Input,
	bool bFinal,
	xhttpdecodeoutputproc pOutput,
	ptr pData
);



/* 返回当前消息的交付模式。 */
XRT_API xhttpdecodemode xrtHttpDecodeMode(
	const xhttpdecode* pDecode
);



/* 判断最终正文边界和全部压缩流 trailer 均已验证。 */
XRT_API bool xrtHttpDecodeDone(const xhttpdecode* pDecode);



/* 返回成功提交给当前消息的线路正文总字节数。 */
XRT_API uint64 xrtHttpDecodeInputSize(const xhttpdecode* pDecode);



/* 返回已经被输出回调接受或明确丢弃的正文总字节数。 */
XRT_API uint64 xrtHttpDecodeOutputSize(const xhttpdecode* pDecode);



/* 销毁解码器；空指针是安全的空操作。 */
XRT_API void xrtHttpDecodeDestroy(xhttpdecode* pDecode);



XRT_EXTERN_C_END

#endif

#endif
