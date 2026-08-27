#ifndef XRT_INTERNAL_HTTP_CORS_CLIENT_H
#define XRT_INTERNAL_HTTP_CORS_CLIENT_H

#include "xrt_http_cors.h"

#include <xrt/http_cors_client.h>



#if defined(XRT_FEATURE_HTTP_CORS_CLIENT)

/* 请求字段检查结果复用安全值溢出和唯一非安全名称统计。 */
typedef struct xrt_http_cors_request_info {
	size_t UnsafeCount;
	bool SafeOverflow;
} xrt_http_cors_request_info;



/* 完整验证请求字段并计算 Fetch 非安全名称集合大小。 */
bool __xrtHttpCorsRequestInspect(
	const xhttpfield* pFields,
	size_t iCount,
	xrt_http_cors_request_info* pInfo
);



/* 从已验证的方法和请求字段检查结果构造预检计划。 */
void __xrtHttpCorsPreflightMake(
	xstrview Method,
	bool bForce,
	const xrt_http_cors_request_info* pInfo,
	xhttpcorspreflightplan* pPlan
);



/* 判断一个指定字段项是否属于最终非安全名称集合。 */
bool __xrtHttpCorsRequestFieldUnsafe(
	const xhttpfield* pFields,
	size_t iCount,
	const xrt_http_cors_request_info* pInfo,
	size_t iIndex
);



/* 从指定位置读取下一个唯一非安全请求字段名。 */
xhttpnext __xrtHttpCorsUnsafeNameNext(
	const xhttpfield* pFields,
	size_t iCount,
	const xrt_http_cors_request_info* pInfo,
	size_t* pOffset,
	xstrview* pName
);

#endif

#endif
