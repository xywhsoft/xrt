#ifndef XRT_INTERNAL_COMPRESS_H
#define XRT_INTERNAL_COMPRESS_H

#include "xrt_internal.h"
#include <xrt/compress.h>



#if defined(XRT_FEATURE_INFLATE) || defined(XRT_FEATURE_DEFLATE)

/* 使用共享小表更新未取反的 CRC32 状态。 */
uint32 __xrtCompressCrc32Update(
	uint32 iCrc,
	const void* pData,
	size_t iSize
);

#endif



#if defined(XRT_FEATURE_DEFLATE)

/* 验证 Deflate 配置，并使用调用方提供的操作名建立结构化错误。 */
bool __xrtDeflateConfigValid(
	const xdeflateconfig* pConfig,
	cstr sOperation
);

#endif



#if defined(XRT_FEATURE_INFLATE)

/* 验证 Inflate 配置，并使用调用方提供的操作名建立结构化错误。 */
bool __xrtInflateConfigValid(
	const xinflateconfig* pConfig,
	cstr sOperation
);

#endif

#endif
