#include "../internal/xrt_http.h"

#include <xrt/http1_tls.h>



#if defined(XRT_FEATURE_HTTP1_TLS)

/* 通过 TLS Stream 的安全入口连续化明文前缀。 */
static bool __xrtHttp1TlsPullup(
	ptr pContext,
	size_t iSize,
	xnetspan* pSpan
)
{
	return xrtTlsStreamPullup(
		(xtlsstream*)pContext,
		iSize,
		pSpan
	);
}



/* 共享请求与响应的 TLS 明文解析入口。 */
static xhttp1status __xrtHttp1TlsParse(
	xtlsstream* pStream,
	xhttp1head* pHead,
	const xhttp1limits* pLimits,
	xhttp1errorinfo* pError,
	__xrt_http1_net_parse_proc pParse
)
{
	const xnetbuf* pBuffer = xrtTlsStreamBuffer(pStream);
	xhttp1status Status;

	if ( pBuffer == NULL ) {
		return XHTTP1_ERROR;
	}
	Status = __xrtHttp1NetParse(
		pBuffer,
		pHead,
		pLimits,
		pError,
		pParse,
		__xrtHttp1TlsPullup,
		pStream
	);
	if ( (Status == XHTTP1_MORE) &&
		(xrtTlsStreamAvailable(pStream) != 0) &&
		!xrtTlsStreamReadMore(pStream) ) {
		return XHTTP1_ERROR;
	}
	return Status;
}



/* 从 TLS 明文块链解析请求 Header。 */
XRT_API xhttp1status xrtHttp1RequestParseTls(
	xtlsstream* pStream,
	xhttp1head* pHead,
	const xhttp1limits* pLimits,
	xhttp1errorinfo* pError
)
{
	return __xrtHttp1TlsParse(
		pStream,
		pHead,
		pLimits,
		pError,
		xrtHttp1RequestParse
	);
}



/* 从 TLS 明文块链解析响应 Header。 */
XRT_API xhttp1status xrtHttp1ResponseParseTls(
	xtlsstream* pStream,
	xhttp1head* pHead,
	const xhttp1limits* pLimits,
	xhttp1errorinfo* pError
)
{
	return __xrtHttp1TlsParse(
		pStream,
		pHead,
		pLimits,
		pError,
		xrtHttp1ResponseParse
	);
}

#endif
