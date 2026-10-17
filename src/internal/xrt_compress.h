#ifndef XRT_INTERNAL_COMPRESS_H
#define XRT_INTERNAL_COMPRESS_H

#include "xrt_internal.h"
#include "xrt_crc32.h"
#include <xrt/compress.h>



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
