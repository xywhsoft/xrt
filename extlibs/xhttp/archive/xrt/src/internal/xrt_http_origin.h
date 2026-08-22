#ifndef XRT_INTERNAL_HTTP_ORIGIN_H
#define XRT_INTERNAL_HTTP_ORIGIN_H

#include "xrt_http.h"
#include "xrt_url.h"

#include <xrt/http_origin.h>



#if defined(XRT_FEATURE_HTTP_ORIGIN)

/* 无错误副作用地验证 Origin 描述符。 */
bool __xrtHttpOriginValueValid(
	const xhttporigin* pOrigin
);



/* 比较两个已经验证的非 null Origin 三元组。 */
bool __xrtHttpOriginTupleSame(
	const xhttporigin* pLeft,
	const xhttporigin* pRight
);



/* 判断一段输出是否覆盖 Origin 描述符借用的任一输入。 */
bool __xrtHttpOriginOverlap(
	const xhttporigin* pOrigin,
	const void* pOutput,
	size_t iSize
);

#endif

#endif
