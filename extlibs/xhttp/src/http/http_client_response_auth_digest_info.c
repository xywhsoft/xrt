#include "../internal/xrt_http_client.h"



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_INFO)

/* 校验并解析唯一 Digest Authentication-Info 响应字段。 */
static xhttpnext __xrtHttpResponseDigestInfo(
	const xhttpresponse* pResponse,
	xstrview Name,
	xhttpdigestalgorithm Algorithm,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestinfo* pInfo
)
{
	const xhttpheaders* pHeaders;
	const xhttpfield* pField = NULL;
	xhttpdigestinfo Empty = { 0 };
	size_t iZero = 0;
	xhttpnext Next;

	if ( !__xrtHttpResponseOutputValid(
		pResponse, pSize, sizeof(*pSize)
	) || !__xrtHttpResponseOutputValid(
		pResponse, pInfo, sizeof(*pInfo)
	) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) && !__xrtHttpResponseOutputValid(
			pResponse, pOutput, iCapacity
		)) || __xrtRangesOverlap(
			pSize, sizeof(*pSize), pInfo, sizeof(*pInfo)
		) || ((pOutput != NULL) && (__xrtRangesOverlap(
			pOutput, iCapacity, pSize, sizeof(*pSize)
		) || __xrtRangesOverlap(
			pOutput, iCapacity, pInfo, sizeof(*pInfo)
		))) ) {
		__xrtHttpResponseSetError(
			XERR_ARGUMENT,
			XHTTP_RESPONSE_ERROR_ARGUMENT,
			"parse-http-response-digest-info",
			"HTTP response Digest info output is invalid",
			NULL
		);
		return XHTTP_NEXT_ERROR;
	}
	pHeaders = xrtHttpResponseHeaders(pResponse);
	memcpy(pSize, &iZero, sizeof(iZero));
	memcpy(pInfo, &Empty, sizeof(Empty));
	Next = xrtHttpHeadersGetUnique(pHeaders, Name, &pField);
	if ( Next == XHTTP_NEXT_END ) {
		return XHTTP_NEXT_END;
	}
	if ( Next == XHTTP_NEXT_ERROR ) {
		xerror* pCause = xrtTakeError();

		__xrtHttpResponseSetError(
			XERR_PROTOCOL,
			XHTTP_RESPONSE_ERROR_HEADER,
			"parse-http-response-digest-info",
			"HTTP response contains multiple Digest info fields",
			pCause
		);
		xrtErrorFree(pCause);
		return XHTTP_NEXT_ERROR;
	}
	if ( !xrtHttpDigestInfoRead(
		pField->Value,
		Algorithm,
		pOutput,
		iCapacity,
		pSize,
		pInfo
	) ) {
		__xrtHttpResponseWrapError(
			XERR_VALUE,
			XHTTP_RESPONSE_ERROR_AUTH,
			"parse-http-response-digest-info",
			"HTTP response Digest authentication info is invalid"
		);
		return XHTTP_NEXT_ERROR;
	}
	return XHTTP_NEXT_ITEM;
}



/* 解析源站 Authentication-Info。 */
XRT_API xhttpnext xrtHttpResponseDigestInfo(
	const xhttpresponse* pResponse,
	xhttpdigestalgorithm Algorithm,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestinfo* pInfo
)
{
	return __xrtHttpResponseDigestInfo(
		pResponse,
		XRT_STR_LITERAL("Authentication-Info"),
		Algorithm,
		pOutput,
		iCapacity,
		pSize,
		pInfo
	);
}



/* 解析代理 Proxy-Authentication-Info。 */
XRT_API xhttpnext xrtHttpResponseProxyDigestInfo(
	const xhttpresponse* pResponse,
	xhttpdigestalgorithm Algorithm,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestinfo* pInfo
)
{
	return __xrtHttpResponseDigestInfo(
		pResponse,
		XRT_STR_LITERAL("Proxy-Authentication-Info"),
		Algorithm,
		pOutput,
		iCapacity,
		pSize,
		pInfo
	);
}

#endif
