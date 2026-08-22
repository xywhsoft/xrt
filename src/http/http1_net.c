#include "../internal/xrt_http.h"

#include <xrt/http1_net.h>



#if defined(XRT_FEATURE_HTTP1_NET)

/* 在块链中定位首个 HTTP 空行，只扫描协议 Header 上限附近的字节。 */
static size_t __xrtHttp1NetHeadEnd(
	const xnetbuf* pBuffer,
	size_t iLimit
)
{
	static const uint8 End[] = { '\r', '\n', '\r', '\n' };
	size_t iAvailable = xrtNetBufSize(pBuffer);
	size_t iScan = iAvailable;
	size_t iOffset = 0;

	if ( iLimit <= (SIZE_MAX - sizeof(End)) ) {
		size_t iBound = iLimit + sizeof(End);

		if ( iScan > iBound ) {
			iScan = iBound;
		}
	}
	while ( iOffset < iScan ) {
		size_t iLine = xrtNetBufFind(
			pBuffer,
			(uint8)'\n',
			iOffset
		);
		uint8 Found[sizeof(End)];

		if ( (iLine == XRT_NPOS) || (iLine >= iScan) ) {
			break;
		}
		if ( (iLine >= 3u) &&
			(xrtNetBufPeek(
				pBuffer,
				iLine - 3u,
				Found,
				sizeof(Found)
			) == sizeof(Found)) &&
			(memcmp(Found, End, sizeof(End)) == 0) ) {
			return iLine + 1u;
		}
		iOffset = iLine + 1u;
	}
	return XRT_NPOS;
}



/* 共享请求和响应的按需连续化路径。 */
xhttp1status __xrtHttp1NetParse(
	const xnetbuf* pBuffer,
	xhttp1head* pHead,
	const xhttp1limits* pInputLimits,
	xhttp1errorinfo* pError,
	__xrt_http1_net_parse_proc pParse,
	__xrt_http1_net_pullup_proc pPullup,
	ptr pContext
)
{
	xhttp1limits Limits;
	xnetspan Span;
	size_t iAvailable;
	size_t iHead;
	size_t iPullup;

	if ( pBuffer == NULL ) {
		return pParse(
			(xbytesview) { NULL, 1u },
			pHead,
			pInputLimits,
			pError
		);
	}
	if ( pInputLimits == NULL ) {
		xrtHttp1LimitsInit(&Limits);
	} else {
		if ( !__xrtRangeValid(pInputLimits, sizeof(Limits)) ) {
			return pParse(
				(xbytesview) { 0 },
				pHead,
				pInputLimits,
				pError
			);
		}
		memcpy(&Limits, pInputLimits, sizeof(Limits));
	}
	iAvailable = xrtNetBufSize(pBuffer);
	iHead = __xrtHttp1NetHeadEnd(pBuffer, Limits.MaxHead);
	if ( iHead != XRT_NPOS ) {
		iPullup = iHead;
	} else if ( iAvailable > Limits.MaxHead ) {
		iPullup = Limits.MaxHead < SIZE_MAX ?
			Limits.MaxHead + 1u : Limits.MaxHead;
	} else if ( xrtNetBufFront(pBuffer, &Span) &&
		(Span.Size == iAvailable) ) {
		return pParse(
			(xbytesview) { Span.Data, Span.Size },
			pHead,
			pInputLimits,
			pError
		);
	} else {
		return pParse(
			(xbytesview) { 0 },
			pHead,
			pInputLimits,
			pError
		);
	}
	if ( (pPullup == NULL) ||
		!pPullup(pContext, iPullup, &Span) ) {
		return XHTTP1_ERROR;
	}
	return pParse(
		(xbytesview) { Span.Data, Span.Size },
		pHead,
		pInputLimits,
		pError
	);
}



/* 连续化普通可变网络缓冲。 */
static bool __xrtHttp1NetBufferPullup(
	ptr pContext,
	size_t iSize,
	xnetspan* pSpan
)
{
	return xrtNetBufPullup(
		(xnetbuf*)pContext,
		iSize,
		pSpan
	);
}



/* 从网络缓冲链解析请求 Header。 */
XRT_API xhttp1status xrtHttp1RequestParseBuffer(
	xnetbuf* pBuffer,
	xhttp1head* pHead,
	const xhttp1limits* pLimits,
	xhttp1errorinfo* pError
)
{
	return __xrtHttp1NetParse(
		pBuffer,
		pHead,
		pLimits,
		pError,
		xrtHttp1RequestParse,
		__xrtHttp1NetBufferPullup,
		pBuffer
	);
}



/* 从网络缓冲链解析响应 Header。 */
XRT_API xhttp1status xrtHttp1ResponseParseBuffer(
	xnetbuf* pBuffer,
	xhttp1head* pHead,
	const xhttp1limits* pLimits,
	xhttp1errorinfo* pError
)
{
	return __xrtHttp1NetParse(
		pBuffer,
		pHead,
		pLimits,
		pError,
		xrtHttp1ResponseParse,
		__xrtHttp1NetBufferPullup,
		pBuffer
	);
}

#endif
