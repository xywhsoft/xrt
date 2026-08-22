#ifndef XRT_INTERNAL_JSON_ESCAPE_H
#define XRT_INTERNAL_JSON_ESCAPE_H

#include "xrt_internal.h"



#if defined(XRT_FEATURE_JSON_ESCAPE)

/* 内部结果让 JSON、XSON 和独立 quote API 各自建立正确错误域。 */
typedef enum xjsonescaperesult {
	XJSON_ESCAPE_OK = 0,
	XJSON_ESCAPE_INVALID,
	XJSON_ESCAPE_OUTPUT,
	XJSON_ESCAPE_OVERFLOW
} xjsonescaperesult;



/* 内部 Writer 直接接收借用范围。 */
typedef bool (*xjsonescapeemitproc)(
	const void* pData,
	size_t iSize,
	ptr pUserData
);



/* 写出完整 JSON 字符串 token，不直接建立格式专属错误。 */
xjsonescaperesult __xrtJsonEscapeWrite(
	xstrview Text,
	uint32 iFlags,
	xjsonescapeemitproc pEmit,
	ptr pUserData,
	size_t* pWritten,
	size_t* pErrorOffset
);

#endif

#endif
