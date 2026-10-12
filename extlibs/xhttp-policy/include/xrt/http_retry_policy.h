#ifndef XHTTP_RETRY_POLICY_H
#define XHTTP_RETRY_POLICY_H

#include <xhttp-policy/export.h>
#include <xrt/http.h>

#if defined(XHTTP_FEATURE_HTTP_RETRY_POLICY) && !defined(XRT_FEATURE_HTTP)
#error "HTTP retry policy requires the HTTP core"
#endif

XRT_EXTERN_C_BEGIN
#if defined(XHTTP_FEATURE_HTTP_RETRY_POLICY)
/* 判断状态码是否属于默认的临时失败重试集合。 */
XHTTP_POLICY_API bool xrtHttpRetryStatusDefault(uint16 iStatus);



/*
	计算以零开始编号的封顶指数退避，单位由调用方决定。
	Maximum 必须非零且 Base 不得大于 Maximum；失败时不修改输出。
*/
XHTTP_POLICY_API bool xrtHttpRetryBackoff(
	uint64 iBase,
	uint64 iMaximum,
	uint32 iRetry,
	uint64* pDelay
);

#endif
XRT_EXTERN_C_END
#endif
