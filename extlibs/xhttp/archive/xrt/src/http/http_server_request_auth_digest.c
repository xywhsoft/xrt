#include "../internal/xrt_http_server.h"



#if defined(XRT_FEATURE_HTTP_SERVER_REQUEST_AUTH_DIGEST)

/* 使用指定字段名读取并解码唯一 Digest 凭据。 */
static xhttpnext __xrtHttpServerRequestDigestAuth(
	const xhttpserverrequest* pRequest,
	xstrview Name,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestauth* pDigest
)
{
	const xhttpfield* pField;
	xhttpdigestauth Digest = { 0 };
	size_t iSize = 0;
	xhttpnext Next;

	if ( !__xrtHttpServerRequestOutputValid(
		pRequest, pSize, sizeof(iSize)
	) || !__xrtHttpServerRequestOutputValid(
		pRequest, pDigest, sizeof(Digest)
	) ||
		((pOutput == NULL) && (iCapacity != 0)) ||
		((pOutput != NULL) &&
		 !__xrtHttpServerRequestOutputValid(
			pRequest, pOutput, iCapacity
		)) || __xrtRangesOverlap(
			pSize, sizeof(iSize), pDigest, sizeof(Digest)
		) || ((pOutput != NULL) && (__xrtRangesOverlap(
			pOutput, iCapacity, pSize, sizeof(iSize)
		) || __xrtRangesOverlap(
			pOutput, iCapacity, pDigest, sizeof(Digest)
		))) ) {
		__xrtHttpServerRequestSetError(
			XERR_ARGUMENT,
			XHTTP_SERVER_REQUEST_ERROR_ARGUMENT,
			"parse-http-server-digest-auth",
			"HTTP Digest authentication output is invalid",
			NULL
		);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pSize, &iSize, sizeof(iSize));
	memcpy(pDigest, &Digest, sizeof(Digest));
	Next = __xrtHttpServerRequestAuthField(
		pRequest, Name, &pField
	);
	if ( Next != XHTTP_NEXT_ITEM ) {
		return Next;
	}
	if ( !xrtHttpDigestAuthRead(
		pField->Value,
		pOutput,
		iCapacity,
		&iSize,
		&Digest
	) ) {
		memcpy(pSize, &iSize, sizeof(iSize));
		memcpy(pDigest, &Digest, sizeof(Digest));
		__xrtHttpServerRequestWrapError(
			XERR_VALUE,
			XHTTP_SERVER_REQUEST_ERROR_AUTH,
			"parse-http-server-digest-auth",
			"HTTP request Digest credentials are invalid"
		);
		return XHTTP_NEXT_ERROR;
	}
	memcpy(pSize, &iSize, sizeof(iSize));
	memcpy(pDigest, &Digest, sizeof(Digest));
	return XHTTP_NEXT_ITEM;
}



/* 解码源站 Digest Authorization。 */
XRT_API xhttpnext xrtHttpServerRequestDigestAuth(
	const xhttpserverrequest* pRequest,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestauth* pDigest
)
{
	return __xrtHttpServerRequestDigestAuth(
		pRequest,
		XRT_STR_LITERAL("Authorization"),
		pOutput,
		iCapacity,
		pSize,
		pDigest
	);
}



/* 解码代理 Digest Proxy-Authorization。 */
XRT_API xhttpnext xrtHttpServerRequestProxyDigestAuth(
	const xhttpserverrequest* pRequest,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize,
	xhttpdigestauth* pDigest
)
{
	return __xrtHttpServerRequestDigestAuth(
		pRequest,
		XRT_STR_LITERAL("Proxy-Authorization"),
		pOutput,
		iCapacity,
		pSize,
		pDigest
	);
}

#endif
