#include "../internal/xrt_internal.h"
#include <xrt/http_retry_policy.h>

#if defined(XHTTP_FEATURE_HTTP_RETRY_POLICY)

/* 使用保守集合识别常见的临时服务或路由失败。 */
XHTTP_POLICY_API bool xrtHttpRetryStatusDefault(uint16 iStatus)
{
	switch ( iStatus ) {
		case XHTTP_STATUS_REQUEST_TIMEOUT:
		case XHTTP_STATUS_MISDIRECTED_REQUEST:
		case XHTTP_STATUS_TOO_EARLY:
		case XHTTP_STATUS_TOO_MANY_REQUESTS:
		case XHTTP_STATUS_INTERNAL_SERVER_ERROR:
		case XHTTP_STATUS_BAD_GATEWAY:
		case XHTTP_STATUS_SERVICE_UNAVAILABLE:
		case XHTTP_STATUS_GATEWAY_TIMEOUT:
			return true;

		default:
			return false;
	}
}



/* 以最多 64 次有效倍增得到不回绕的封顶指数退避。 */
XHTTP_POLICY_API bool xrtHttpRetryBackoff(
	uint64 iBase,
	uint64 iMaximum,
	uint32 iRetry,
	uint64* pDelay
)
{
	uint64 iDelay = iBase;
	uint32 i;

	if ( !xrtMemRangeValid(pDelay, sizeof(iDelay)) ||
		(iMaximum == 0) ||
		(iBase > iMaximum) ) {
		xrtSetErrorKind(XERR_ARGUMENT);
		return false;
	}
	/* 零基数的全部指数项都为零，避免按外部序号空转。 */
	if ( iBase == 0 ) {
		memcpy(pDelay, &iDelay, sizeof(iDelay));
		return true;
	}
	for ( i = 0; (i < iRetry) && (iDelay < iMaximum); i++ ) {
		if ( iDelay > (iMaximum / UINT64_C(2)) ) {
			iDelay = iMaximum;
		} else {
			iDelay *= UINT64_C(2);
		}
	}
	memcpy(pDelay, &iDelay, sizeof(iDelay));
	return true;
}

#endif
