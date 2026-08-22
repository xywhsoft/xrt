#include "../internal/xrt_http_client.h"

#include <xrt/memory.h>



#if defined(XHTTP_FEATURE_HTTP_CLIENT_RESPONSE_AUTH_DIGEST_SESSION)

/* Digest 回执读取函数保持源站与代理路径完全对称。 */
typedef xhttpnext (*xrt_http_digest_session_info_function)(
	const xhttpresponse* pResponse,
	xhttpdigestalgorithm Algorithm,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestinfo* pInfo
);



/* 解析唯一回执，并把会话状态转换保持为一次失败原子操作。 */
static xhttpnext __xrtHttpResponseDigestSessionAccept(
	const xhttpresponse* pResponse,
	xhttpdigestsession* pSession,
	const xhttpdigestexchange* pExchange,
	xstrview ResponseEntityHash,
	xstrview NextCnonce,
	xhttpdigestsessioncheck* pCheck,
	xrt_http_digest_session_info_function pInfo,
	cstr sOperation
)
{
	const xhttpdigestproof* pProof;
	xhttpdigestsessioncheck Check = XHTTP_DIGEST_SESSION_ERROR;
	xhttpdigestinfo Info;
	xhttpnext Next;
	void* pOutput;
	size_t iSize = 0;

	if ( (pSession == NULL) || (pExchange == NULL) ||
		!__xrtHttpViewValid(ResponseEntityHash) ||
		!__xrtHttpViewValid(NextCnonce) ||
		!__xrtHttpResponseOutputValid(
			pResponse, pCheck, sizeof(Check)
		) || !__xrtHttpDigestSessionOutputValid(
			pSession, pExchange, pCheck, sizeof(Check)
		) ||
		__xrtRangesOverlap(
			pCheck, sizeof(Check),
			ResponseEntityHash.Data, ResponseEntityHash.Size
		) || __xrtRangesOverlap(
			pCheck, sizeof(Check),
			NextCnonce.Data, NextCnonce.Size
		) ) {
		__xrtHttpResponseSetError(
			XERR_ARGUMENT,
			XHTTP_RESPONSE_ERROR_ARGUMENT,
			sOperation,
			"HTTP response Digest session input is invalid",
			NULL
		);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pCheck, &Check, sizeof(Check));
	pProof = xrtHttpDigestExchangeProof(pExchange);
	if ( pProof == NULL ) {
		__xrtHttpResponseWrapError(
			XERR_ARGUMENT,
			XHTTP_RESPONSE_ERROR_AUTH,
			sOperation,
			"HTTP response Digest exchange is invalid"
		);
		return XHTTP_NEXT_ERROR;
	}
	Next = pInfo(
		pResponse,
		pProof->Algorithm,
		NULL,
		0,
		&iSize,
		&Info
	);
	if ( Next != XHTTP_NEXT_ITEM ) {
		return Next;
	}
	pOutput = xrtMalloc(iSize);
	if ( pOutput == NULL ) {
		__xrtHttpResponseWrapError(
			XERR_MEMORY,
			XHTTP_RESPONSE_ERROR_AUTH,
			sOperation,
			"HTTP response Digest info allocation failed"
		);
		return XHTTP_NEXT_ERROR;
	}
	Next = pInfo(
		pResponse,
		pProof->Algorithm,
		pOutput,
		iSize,
		&iSize,
		&Info
	);
	if ( Next == XHTTP_NEXT_ITEM ) {
		Check = xrtHttpDigestSessionAccept(
			pSession,
			pExchange,
			&Info,
			ResponseEntityHash,
			NextCnonce
		);
	}
	xrtSecureZero(pOutput, iSize);
	xrtFree(pOutput);
	if ( Next != XHTTP_NEXT_ITEM ) {
		return Next;
	}
	if ( Check == XHTTP_DIGEST_SESSION_ERROR ) {
		__xrtHttpResponseWrapError(
			XERR_PROTOCOL,
			XHTTP_RESPONSE_ERROR_AUTH,
			sOperation,
			"HTTP response Digest session verification failed"
		);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pCheck, &Check, sizeof(Check));
	return XHTTP_NEXT_ITEM;
}



/* 解析并接受源站 Authentication-Info。 */
XRT_API xhttpnext xrtHttpResponseDigestSessionAccept(
	const xhttpresponse* pResponse,
	xhttpdigestsession* pSession,
	const xhttpdigestexchange* pExchange,
	xstrview ResponseEntityHash,
	xstrview NextCnonce,
	xhttpdigestsessioncheck* pCheck
)
{
	return __xrtHttpResponseDigestSessionAccept(
		pResponse,
		pSession,
		pExchange,
		ResponseEntityHash,
		NextCnonce,
		pCheck,
		xrtHttpResponseDigestInfo,
		"accept-http-response-digest-session"
	);
}



/* 解析并接受代理 Proxy-Authentication-Info。 */
XRT_API xhttpnext xrtHttpResponseProxyDigestSessionAccept(
	const xhttpresponse* pResponse,
	xhttpdigestsession* pSession,
	const xhttpdigestexchange* pExchange,
	xstrview ResponseEntityHash,
	xstrview NextCnonce,
	xhttpdigestsessioncheck* pCheck
)
{
	return __xrtHttpResponseDigestSessionAccept(
		pResponse,
		pSession,
		pExchange,
		ResponseEntityHash,
		NextCnonce,
		pCheck,
		xrtHttpResponseProxyDigestInfo,
		"accept-http-response-proxy-digest-session"
	);
}

#endif
