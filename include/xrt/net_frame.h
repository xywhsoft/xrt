#ifndef XRT_NET_FRAME_H
#define XRT_NET_FRAME_H

#include <xrt/net.h>



#if defined(XRT_FEATURE_NET_FRAME) && \
	!defined(XRT_FEATURE_NET_BUFFER)
	#error "XRT network framing requires XRT_FEATURE_NET_BUFFER"
#endif

#if defined(XRT_FEATURE_NET_FRAME_LINE) && \
	!defined(XRT_FEATURE_NET_FRAME)
	#error "XRT line framing requires XRT_FEATURE_NET_FRAME"
#endif

#if defined(XRT_FEATURE_NET_FRAME_LENGTH) && \
	!defined(XRT_FEATURE_NET_FRAME)
	#error "XRT length framing requires XRT_FEATURE_NET_FRAME"
#endif



#if defined(XRT_FEATURE_NET_FRAME)

/* 增量 framing 只区分失败、等待更多字节和完整帧。 */
typedef enum xnetframestatus {
	XNET_FRAME_ERROR = -1,
	XNET_FRAME_MORE = 0,
	XNET_FRAME_READY = 1
} xnetframestatus;



/* 所有偏移都相对当前输入头部，Declared 保存协议字段原值。 */
typedef struct xnetframe {
	size_t PayloadOffset;
	size_t PayloadSize;
	size_t FrameSize;
	uint64 Declared;
} xnetframe;

#endif



#if defined(XRT_FEATURE_NET_FRAME_LINE)

/* 分隔符只借用调用方字节，并且必须存活到 Framer 不再使用。 */
typedef struct xnetlineconfig {
	xbytesview Delimiter;
	size_t MaxPayload;
	bool IncludeDelimiter;
} xnetlineconfig;



/* Line Framer 保存块内增量游标，字段公开只用于无分配栈存储。 */
typedef struct xnetlineframer {
	xnetlineconfig Config;
	const xnetbuf* Input;
	xnetblock* Cursor;
	size_t CursorOffset;
	size_t Search;
	size_t PreviousSize;
	uint32 Guard;
} xnetlineframer;

#endif



#if defined(XRT_FEATURE_NET_FRAME_LENGTH)

/* 长度字段字节序独立于主机字节序。 */
typedef enum xnetframeorder {
	XNET_FRAME_BIG_ENDIAN = 0,
	XNET_FRAME_LITTLE_ENDIAN = 1
} xnetframeorder;



/* FrameSize = LengthOffset + LengthSize + Declared + Adjustment。 */
typedef struct xnetlengthconfig {
	size_t LengthOffset;
	size_t LengthSize;
	int64 Adjustment;
	size_t Strip;
	size_t MaxFrame;
	xnetframeorder Order;
} xnetlengthconfig;



/* Length Framer 复制并验证配置，解析本身不保存输入状态。 */
typedef struct xnetlengthframer {
	xnetlengthconfig Config;
	uint32 Guard;
} xnetlengthframer;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_NET_FRAME)

/* 从完整帧复制不超过输出容量的 payload 字节。 */
XRT_API size_t xrtNetFrameCopy(
	const xnetbuf* pInput,
	const xnetframe* pFrame,
	void* pOutput,
	size_t iCapacity
);



/* 只有帧范围仍完整位于输入头部时才精确消费 FrameSize 字节。 */
XRT_API bool xrtNetFrameConsume(
	xnetbuf* pInput,
	const xnetframe* pFrame
);

#endif



#if defined(XRT_FEATURE_NET_FRAME_LINE)

/* 初始化 LF、8192 字节 payload 上限和去除分隔符的默认配置。 */
XRT_API void xrtNetLineConfigInit(xnetlineconfig* pConfig);



/* 复制配置并开始一条新的增量行帧搜索。 */
XRT_API bool xrtNetLineInit(
	xnetlineframer* pFramer,
	const xnetlineconfig* pConfig
);



/* 保留配置并丢弃当前增量搜索进度。 */
XRT_API bool xrtNetLineReset(xnetlineframer* pFramer);



/* 解析输入头部的下一条分隔帧；MORE 后只允许保留前缀并追加输入。 */
XRT_API xnetframestatus xrtNetLineNext(
	xnetlineframer* pFramer,
	const xnetbuf* pInput,
	xnetframe* pFrame
);

#endif



#if defined(XRT_FEATURE_NET_FRAME_LENGTH)

/* 初始化四字节大端长度、去除长度字段和 1 MiB 帧上限。 */
XRT_API void xrtNetLengthConfigInit(xnetlengthconfig* pConfig);



/* 复制并验证长度字段偏移、宽度、调整、strip 和帧上限。 */
XRT_API bool xrtNetLengthInit(
	xnetlengthframer* pFramer,
	const xnetlengthconfig* pConfig
);



/* 从输入头部解析下一条长度前缀帧。 */
XRT_API xnetframestatus xrtNetLengthNext(
	const xnetlengthframer* pFramer,
	const xnetbuf* pInput,
	xnetframe* pFrame
);

#endif



XRT_EXTERN_C_END

#endif
