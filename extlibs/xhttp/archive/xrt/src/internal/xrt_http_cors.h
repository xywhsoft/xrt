#ifndef XRT_INTERNAL_HTTP_CORS_H
#define XRT_INTERNAL_HTTP_CORS_H

#include "xrt_http_origin.h"

#include <xrt/http_cors_policy.h>



#if defined(XRT_FEATURE_HTTP_CORS_POLICY)

/* 无错误副作用地验证决策与其请求头来源。 */
bool __xrtHttpCorsDecisionValid(
	const xhttpcorsdecision* pDecision,
	const xhttpfield* pRequestFields,
	size_t iRequestFieldCount
);



/* 判断一段内存是否覆盖决策描述符或任一借用输入。 */
bool __xrtHttpCorsDecisionOverlap(
	const xhttpcorsdecision* pDecision,
	const xhttpfield* pRequestFields,
	size_t iRequestFieldCount,
	const void* pMemory,
	size_t iSize
);

#endif

#endif
