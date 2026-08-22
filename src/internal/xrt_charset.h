#ifndef XRT_INTERNAL_CHARSET_H
#define XRT_INTERNAL_CHARSET_H

#include "xrt_internal.h"



#if defined(XRT_FEATURE_UNICODE)

/* 内部解码结果包含替换模式应消费的最大子部件长度。 */
typedef struct xrt_utf_decode {
	xutfstatus Status;
	uint32 Scalar;
	size_t Read;
} xrt_utf_decode;



/* 严格解码一个 UTF-8 标量。 */
xrt_utf_decode __xrtUtf8Decode(const unsigned char* pText, size_t iSize);



/* 严格解码一个 UTF-16 标量。 */
xrt_utf_decode __xrtUtf16Decode(const uint16* pText, size_t iSize);



/* 严格解码一个 UTF-32 标量。 */
xrt_utf_decode __xrtUtf32Decode(const uint32* pText, size_t iSize);



/* 无错误副作用地返回 UTF-8 编码长度并可写出结果。 */
size_t __xrtUtf8Encode(uint32 iScalar, unsigned char* pOutput);



/* 无错误副作用地返回 UTF-16 编码长度并可写出结果。 */
size_t __xrtUtf16Encode(uint32 iScalar, uint16* pOutput);



/* 设置带错误位置的 Unicode 值错误。 */
void __xrtUtfSetInvalid(cstr sOperation, size_t iOffset);



/* 设置 Unicode 转换大小溢出错误。 */
void __xrtUtfSetOverflow(cstr sOperation);

#endif



#if defined(XRT_FEATURE_CHARSET)

/* 按显式字节序读取 16 位码元，避免未对齐访问并供转码与检测共用。 */
static inline uint16 __xrtCharsetRead16(
	const unsigned char* pData,
	bool bBigEndian
)
{
	if ( bBigEndian ) {
		return (uint16)(((uint16)pData[0] << 8) | (uint16)pData[1]);
	}
	return __xrtReadLe16(pData);
}



/* 按显式字节序读取 32 位码元，避免未对齐访问并供转码与检测共用。 */
static inline uint32 __xrtCharsetRead32(
	const unsigned char* pData,
	bool bBigEndian
)
{
	if ( bBigEndian ) {
		return ((uint32)pData[0] << 24) | ((uint32)pData[1] << 16) |
			((uint32)pData[2] << 8) | (uint32)pData[3];
	}
	return __xrtReadLe32(pData);
}

#endif

#endif
