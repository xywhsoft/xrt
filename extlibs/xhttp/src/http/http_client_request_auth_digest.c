#include "../internal/xrt_http_client.h"



#if defined(XHTTP_FEATURE_HTTP_CLIENT_REQUEST_AUTH_DIGEST)

/* 把 Digest 凭据交给协议层规范写出器。 */
static bool __xrtHttpRequestDigestWrite(
	const void* pContext,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	return xrtHttpDigestAuthWrite(
		(const xhttpdigestauth*)pContext,
		pOutput,
		iCapacity,
		pSize
	);
}



/* 使用指定字段名原子设置 Digest 凭据。 */
static bool __xrtHttpRequestSetDigestAuth(
	xhttprequest* pRequest,
	xstrview Name,
	const xhttpdigestauth* pDigest
)
{
	return __xrtHttpRequestSetWrittenAuth(
		pRequest,
		Name,
		__xrtHttpRequestDigestWrite,
		pDigest
	);
}



/* 设置源站 Digest Authorization。 */
XRT_API bool xrtHttpRequestSetDigestAuth(
	xhttprequest* pRequest,
	const xhttpdigestauth* pDigest
)
{
	return __xrtHttpRequestSetDigestAuth(
		pRequest,
		XRT_STR_LITERAL("Authorization"),
		pDigest
	);
}



/* 设置代理 Digest Proxy-Authorization。 */
XRT_API bool xrtHttpRequestSetProxyDigestAuth(
	xhttprequest* pRequest,
	const xhttpdigestauth* pDigest
)
{
	return __xrtHttpRequestSetDigestAuth(
		pRequest,
		XRT_STR_LITERAL("Proxy-Authorization"),
		pDigest
	);
}

#endif
