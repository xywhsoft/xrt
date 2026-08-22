#ifndef XRT_COMPRESS_H
#define XRT_COMPRESS_H

#include <xrt/error.h>
#include <xrt/memory.h>



/*
	策略枚举参与 WebSocket 的稳定公开配置布局，因此不随 Deflate 实现裁剪。
	关闭压缩实现时它只提供类型信息，不引入任何运行时代码。
*/
typedef enum xdeflatestrategy {
	XDEFLATE_STRATEGY_DEFAULT = 0,
	XDEFLATE_STRATEGY_FILTERED,
	XDEFLATE_STRATEGY_HUFFMAN,
	XDEFLATE_STRATEGY_RLE,
	XDEFLATE_STRATEGY_FIXED
} xdeflatestrategy;



#if defined(XRT_FEATURE_INFLATE)

#define XINFLATE_OUTPUT_UNLIMITED UINT64_MAX
#define XINFLATE_GZIP_HEADER_DEFAULT UINT32_C(65536)
#define XINFLATE_WINDOW_MIN 8u
#define XINFLATE_WINDOW_MAX 15u



/* Inflate 支持原始 DEFLATE、zlib、兼容 HTTP deflate 和 gzip 数据流。 */
typedef enum xinflateformat {
	XINFLATE_RAW = 0,
	XINFLATE_ZLIB,
	XINFLATE_DEFLATE,
	XINFLATE_GZIP
} xinflateformat;



/* Inflate 错误码区分配置、状态、数据、限额和输出消费者失败。 */
typedef enum xinflateerror {
	XINFLATE_ERROR_ARGUMENT = 1,
	XINFLATE_ERROR_CONFIG,
	XINFLATE_ERROR_STATE,
	XINFLATE_ERROR_DATA,
	XINFLATE_ERROR_LIMIT,
	XINFLATE_ERROR_OUTPUT
} xinflateerror;



/*
	OutputLimit 是所有 gzip member 或单个 DEFLATE 流的解码总上限。
	GzipHeaderLimit 限制每个 gzip member 的固定头和可选字段总长度。
	WindowBits 接受 8 到 15，并严格限制允许引用的历史距离。
*/
typedef struct xinflateconfig {
	xinflateformat Format;
	uint64 OutputLimit;
	uint32 GzipHeaderLimit;
	uint8 WindowBits;
} xinflateconfig;



/* Inflate 对象按需拥有一个算法必需的 32 KiB 滑动窗口，并可复位复用。 */
typedef struct xinflate xinflate;



/*
	输出视图只在回调期间有效；返回 false 会使当前 Inflate 进入失败终态。
	回调可设置更具体的当前错误，未设置时由 Inflate 建立输出错误。
*/
typedef bool (*xinflateoutputproc)(xbytesview Data, ptr pData);

#endif



#if defined(XRT_FEATURE_DEFLATE)

#define XDEFLATE_OUTPUT_UNLIMITED UINT64_MAX
#define XDEFLATE_LEVEL_DEFAULT 6
#define XDEFLATE_WINDOW_MIN 8u
#define XDEFLATE_WINDOW_MAX 15u



/* Deflate 输出可选择原始数据流、zlib 包装或确定性 gzip member。 */
typedef enum xdeflateformat {
	XDEFLATE_RAW = 0,
	XDEFLATE_ZLIB,
	XDEFLATE_GZIP
} xdeflateformat;



/* Flush 决定是否只推进、同步边界、清空历史匹配或结束完整数据流。 */
typedef enum xdeflateflush {
	XDEFLATE_FLUSH_NONE = 0,
	XDEFLATE_FLUSH_SYNC,
	XDEFLATE_FLUSH_FULL,
	XDEFLATE_FLUSH_FINISH
} xdeflateflush;



/* Deflate 错误码区分参数、配置、状态、限额、消费者和编码器异常。 */
typedef enum xdeflateerror {
	XDEFLATE_ERROR_ARGUMENT = 1,
	XDEFLATE_ERROR_CONFIG,
	XDEFLATE_ERROR_STATE,
	XDEFLATE_ERROR_LIMIT,
	XDEFLATE_ERROR_OUTPUT,
	XDEFLATE_ERROR_CODEC
} xdeflateerror;



/*
	Level 接受 0 到 10；WindowBits 接受 8 到 15。
	OutputLimit 包含 zlib 或 gzip 包装字节。
	默认配置使用 gzip、级别 6、默认策略和无限输出。
*/
typedef struct xdeflateconfig {
	xdeflateformat Format;
	int32 Level;
	xdeflatestrategy Strategy;
	uint64 OutputLimit;
	uint8 WindowBits;
} xdeflateconfig;



/* Deflate 对象按需拥有算法字典和编码表，并可复位复用。 */
typedef struct xdeflate xdeflate;



/*
	输出视图只在回调期间有效；返回 false 会使当前 Deflate 进入失败终态。
	回调可设置更具体的当前错误，未设置时由 Deflate 建立输出错误。
*/
typedef bool (*xdeflateoutputproc)(xbytesview Data, ptr pData);

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_INFLATE)

/* 初始化默认配置；目标只需是有效连续存储，不要求自然对齐。 */
XRT_API void xrtInflateConfigInit(xinflateconfig* pConfig);



/* 验证 Inflate 配置；输入只需是有效连续存储，不要求自然对齐。 */
XRT_API bool xrtInflateConfigValid(const xinflateconfig* pConfig);



/* 创建流式解码器；配置为空时使用默认值，否则立即复制配置快照。 */
XRT_API xinflate* xrtInflateCreate(const xinflateconfig* pConfig);



/* 失败原子地复位解码器并保留已经分配的滑动窗口。 */
XRT_API bool xrtInflateReset(
	xinflate* pInflate,
	const xinflateconfig* pConfig
);



/*
	同步消费完整输入片段并把输出分段交给回调；Output 为空时丢弃输出。
	Final 表示不会再提供输入，成功时要求压缩流完整结束且校验通过。
	Input 必须表示有效连续范围；参数失败不会改变解码器状态。
*/
XRT_API bool xrtInflateWrite(
	xinflate* pInflate,
	xbytesview Input,
	bool bFinal,
	xinflateoutputproc pOutput,
	ptr pData
);



/* 判断解码器是否已经完整结束；失败状态返回 false。 */
XRT_API bool xrtInflateDone(const xinflate* pInflate);



/* 返回当前流已经产生的解码字节总数。 */
XRT_API uint64 xrtInflateOutputSize(const xinflate* pInflate);



/* 销毁解码器；空指针为空操作，输出回调中的同对象销毁会被拒绝。 */
XRT_API void xrtInflateDestroy(xinflate* pInflate);



/*
	一次性解码完整输入并返回由 xrtFree 释放的字节。
	结果额外带一个不计入 OutputSize 的零字节；输出长度槽无需自然对齐。
	Input 或输出长度槽无效时失败，任何失败都不修改 OutputSize。
*/
XRT_API bytes xrtInflateAll(
	xbytesview Input,
	const xinflateconfig* pConfig,
	size_t* pOutputSize
);

#endif



#if defined(XRT_FEATURE_DEFLATE)

/* 初始化默认配置；目标只需是有效连续存储，不要求自然对齐。 */
XRT_API void xrtDeflateConfigInit(xdeflateconfig* pConfig);



/* 验证 Deflate 配置；输入只需是有效连续存储，不要求自然对齐。 */
XRT_API bool xrtDeflateConfigValid(const xdeflateconfig* pConfig);



/* 创建流式编码器；配置为空时使用默认值，否则立即复制配置快照。 */
XRT_API xdeflate* xrtDeflateCreate(
	const xdeflateconfig* pConfig
);



/* 失败原子地复位编码器，并保留已经分配的算法状态存储。 */
XRT_API bool xrtDeflateReset(
	xdeflate* pDeflate,
	const xdeflateconfig* pConfig
);



/*
	同步消费完整输入片段并把输出分段交给回调；Output 为空时丢弃输出。
	FINISH 成功后对象进入完成终态；SYNC 和 FULL 保持数据流可继续写入。
	Input 必须表示有效连续范围；参数失败不会改变编码器状态。
*/
XRT_API bool xrtDeflateWrite(
	xdeflate* pDeflate,
	xbytesview Input,
	xdeflateflush Flush,
	xdeflateoutputproc pOutput,
	ptr pData
);



/* 判断编码器是否已经通过 FINISH 完整结束；失败状态返回 false。 */
XRT_API bool xrtDeflateDone(const xdeflate* pDeflate);



/* 返回当前数据流已经成功交付的编码字节总数。 */
XRT_API uint64 xrtDeflateOutputSize(
	const xdeflate* pDeflate
);



/* 销毁编码器；空指针为空操作，输出回调中的同对象销毁会被拒绝。 */
XRT_API void xrtDeflateDestroy(xdeflate* pDeflate);



/*
	一次性编码完整输入并返回由 xrtFree 释放的字节。
	结果额外带一个不计入 OutputSize 的零字节；输出长度槽无需自然对齐。
	Input 或输出长度槽无效时失败，任何失败都不修改 OutputSize。
*/
XRT_API bytes xrtDeflateAll(
	xbytesview Input,
	const xdeflateconfig* pConfig,
	size_t* pOutputSize
);

#endif



XRT_EXTERN_C_END

#endif
