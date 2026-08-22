#include "../internal/xrt_http_server.h"



#if defined(XHTTP_FEATURE_HTTP_SERVER_REQUEST_AUTH_BASIC)

/* 使用指定字段名读取并解码 Basic 凭据。 */
static xhttpnext __xrtHttpServerRequestBasicAuth(
	const xhttpserverrequest* pRequest,
	xstrview Name,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpbasicauth* pBasic
)
{
	xhttpbasicauth Basic = { 0 };
	const xhttpfield* pField;
	size_t iSize = 0;
	size_t iZero = 0;
	xhttpnext Next;

	if ( !__xrtHttpServerRequestOutputValid(
		pRequest, pSize, sizeof(iSize)
	) || !__xrtHttpServerRequestOutputValid(
		pRequest, pBasic, sizeof(Basic)
	) || ((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) &&
		 !__xrtHttpServerRequestOutputValid(
			pRequest, pOutput, iCapacity
		)) || __xrtRangesOverlap(
		pSize, sizeof(iSize), pBasic, sizeof(Basic)
	) || ((pOutput != NULL) && (__xrtRangesOverlap(
		pOutput, iCapacity, pSize, sizeof(iSize)
	) || __xrtRangesOverlap(
		pOutput, iCapacity, pBasic, sizeof(Basic)
	))) ) {
		__xrtHttpServerRequestSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_REQUEST_ERROR_ARGUMENT,
			"parse-http-server-basic-auth",
			"HTTP Basic authentication output is invalid",
			NULL
		);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pBasic, &Basic, sizeof(Basic));
	Next = __xrtHttpServerRequestAuthField(
		pRequest,
		Name,
		&pField
	);
	if ( Next != XHTTP_NEXT_ITEM ) {
		if ( Next == XHTTP_NEXT_END ) {
			memcpy(pSize, &iZero, sizeof(iZero));
		}
		return Next;
	}
	if ( !xrtHttpBasicRead(
		pField->Value,
		pOutput,
		iCapacity,
		&iSize,
		&Basic
	) ) {
		if ( iSize != 0 ) {
			memcpy(pSize, &iSize, sizeof(iSize));
		}
		memcpy(pBasic, &Basic, sizeof(Basic));
		__xrtHttpServerRequestWrapError(
			XERR_VALUE,
			XHTTP_SERVER_REQUEST_ERROR_AUTH,
			"parse-http-server-basic-auth",
			"HTTP request Basic credentials are invalid"
		);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pSize, &iSize, sizeof(iSize));
	memcpy(pBasic, &Basic, sizeof(Basic));
	return XHTTP_NEXT_ITEM;
}



/* 解码源站 Basic Authorization。 */
XRT_API xhttpnext xrtHttpServerRequestBasicAuth(
	const xhttpserverrequest* pRequest,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpbasicauth* pBasic
)
{
	return __xrtHttpServerRequestBasicAuth(
		pRequest,
		XRT_STR_LITERAL("Authorization"),
		pOutput,
		iCapacity,
		pSize,
		pBasic
	);
}



/* 解码代理 Basic Proxy-Authorization。 */
XRT_API xhttpnext xrtHttpServerRequestProxyBasicAuth(
	const xhttpserverrequest* pRequest,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpbasicauth* pBasic
)
{
	return __xrtHttpServerRequestBasicAuth(
		pRequest,
		XRT_STR_LITERAL("Proxy-Authorization"),
		pOutput,
		iCapacity,
		pSize,
		pBasic
	);
}

#endif
