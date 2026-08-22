#ifndef XRT_BUFFER_H
#define XRT_BUFFER_H

#include <xrt/array.h>

#if defined(XRT_FEATURE_BUFFER_HEX) || defined(XRT_FEATURE_BUFFER_BASE64)
	#include <xrt/codec.h>
#endif



#if defined(XRT_FEATURE_BUFFER) && !defined(XRT_FEATURE_ARRAY)
	#error "XRT_FEATURE_BUFFER requires XRT_FEATURE_ARRAY"
#endif

#if defined(XRT_FEATURE_BUFFER_HEX) && \
	(!defined(XRT_FEATURE_BUFFER) || !defined(XRT_FEATURE_CODEC_HEX))
	#error "XRT_FEATURE_BUFFER_HEX requires XRT_FEATURE_BUFFER and XRT_FEATURE_CODEC_HEX"
#endif

#if defined(XRT_FEATURE_BUFFER_BASE64) && \
	(!defined(XRT_FEATURE_BUFFER) || !defined(XRT_FEATURE_CODEC_BASE64))
	#error "XRT_FEATURE_BUFFER_BASE64 requires XRT_FEATURE_BUFFER and XRT_FEATURE_CODEC_BASE64"
#endif



#if defined(XRT_FEATURE_BUFFER)

/* 连续字节缓冲拥有 Data，Size 以内是有效内容，Capacity 以内可直接写入。 */
typedef struct xbuffer {
	bytes Data;
	size_t Size;
	size_t Capacity;
} xbuffer;



XRT_EXTERN_C_BEGIN



/* 初始化调用方持有的空缓冲。 */
XRT_API bool xrtBufferInit(xbuffer* pBuffer);



/* 创建空缓冲。 */
XRT_API xbuffer* xrtBufferCreate(void);



/* 释放缓冲持有的连续内存，但不释放缓冲结构。 */
XRT_API void xrtBufferUnit(xbuffer* pBuffer);



/* 释放缓冲持有的连续内存和缓冲结构。 */
XRT_API void xrtBufferDestroy(xbuffer* pBuffer);



/* 清空有效内容但保留容量。 */
XRT_API void xrtBufferClear(xbuffer* pBuffer);



/* 返回当前有效内容的借用视图。 */
XRT_API xbytesview xrtBufferView(const xbuffer* pBuffer);



/* 保证缓冲至少具有指定容量，实际容量可以按几何策略增长。 */
XRT_API bool xrtBufferReserve(xbuffer* pBuffer, size_t iCapacity);



/* 调整有效长度，扩展区域全部填零，缩小时保留容量。 */
XRT_API bool xrtBufferResize(xbuffer* pBuffer, size_t iSize);



/* 把容量精确裁剪到有效长度，空缓冲会释放存储。 */
XRT_API bool xrtBufferTrim(xbuffer* pBuffer);



/* 在末尾增加未初始化字节并返回首地址，大小必须大于零。 */
XRT_API bytes xrtBufferAdd(xbuffer* pBuffer, size_t iSize);



/* 在指定位点插入未初始化字节并返回首地址，大小必须大于零。 */
XRT_API bytes xrtBufferInsertSpace(
	xbuffer* pBuffer,
	size_t iOffset,
	size_t iSize
);



/* 用字节视图替换全部有效内容，失败时保留原缓冲。 */
XRT_API bool xrtBufferAssign(xbuffer* pBuffer, xbytesview Data);



/* 复制追加字节视图，允许来源是缓冲自身的有效子视图。 */
XRT_API bool xrtBufferAppend(xbuffer* pBuffer, xbytesview Data);



/* 追加一个字节。 */
XRT_API bool xrtBufferAppendByte(xbuffer* pBuffer, uint8 iByte);



/* 在指定位点复制插入字节，允许来源是缓冲自身的有效子视图。 */
XRT_API bool xrtBufferInsert(
	xbuffer* pBuffer,
	size_t iOffset,
	xbytesview Data
);



/*
	从指定位点覆盖字节；末端超出当前长度时扩展并把中间空洞填零。
	允许来源是缓冲自身的有效子视图。
*/
XRT_API bool xrtBufferWrite(
	xbuffer* pBuffer,
	size_t iOffset,
	xbytesview Data
);



/* 删除完整有效区间，不会静默截断到末尾。 */
XRT_API bool xrtBufferRemove(
	xbuffer* pBuffer,
	size_t iOffset,
	size_t iSize
);



/*
	接管由 xrtMalloc 家族分配的连续内存。
	成功时清空来源槽并释放缓冲原有内存，失败时双方所有权和内容都不变。
*/
XRT_API bool xrtBufferSetTake(
	xbuffer* pBuffer,
	bytes* pData,
	size_t iSize,
	size_t iCapacity
);



/*
	取走连续内存并把缓冲重置为空；空缓冲成功返回 NULL。
	长度和容量输出可为空，但不得位于缓冲持有的内存中或互相重叠。
*/
XRT_API bytes xrtBufferTake(
	xbuffer* pBuffer,
	size_t* pSize,
	size_t* pCapacity
);



/* 创建字节视图的独立副本。 */
XRT_API xbuffer* xrtBufferFrom(xbytesview Data);



/* 创建缓冲并接管来源槽，失败时来源所有权不变。 */
XRT_API xbuffer* xrtBufferCreateTake(
	bytes* pData,
	size_t iSize,
	size_t iCapacity
);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_BUFFER_HEX)

XRT_EXTERN_C_BEGIN



/* 严格解码 HEX 文本并创建缓冲。 */
XRT_API xbuffer* xrtBufferFromHex(xstrview Text, uint32 iFlags);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_BUFFER_BASE64)

XRT_EXTERN_C_BEGIN



/* 按 Base64 配置严格解码文本并创建缓冲。 */
XRT_API xbuffer* xrtBufferFromBase64(
	xstrview Text,
	const xbase64config* pConfig
);



XRT_EXTERN_C_END

#endif

#endif
